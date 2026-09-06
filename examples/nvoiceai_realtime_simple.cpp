#include <iostream>
#include <thread>
#include <cstring>
#include <chrono>
#include <cmath>
#include <filesystem>
#include "NvoiceAiSdk.h"
#include "ConfigManager.h"

#ifdef SAVE_AUDIO_FILES
#include <sndfile.h>
#endif

namespace fs = std::filesystem;

// Demo duration in seconds (can be modified to change how long the demo runs)
#define DEMO_DURATION_SECONDS 60

// Interval in milliseconds for checking demo duration
#define DEMO_CHECK_INTERVAL_MS 500

// Interval in seconds for printing frame statistics
#define FRAME_PRINT_INTERVAL_SECONDS 5

// Global variables for audio file handling
#ifdef SAVE_AUDIO_FILES
static SNDFILE* mic_file = nullptr;
static SNDFILE* near_file = nullptr;
static SNDFILE* far_file = nullptr;
static SNDFILE* mix_file = nullptr;
static SF_INFO sf_info;
static std::chrono::steady_clock::time_point last_print_time;
#endif

static std::chrono::steady_clock::time_point callback_start_time;
static size_t frame_count = 0;

#ifdef SAVE_AUDIO_FILES
// Initialize WAV file writers
static bool init_wav_files(const std::string& output_dir, int sample_rate) {
    // Create output directory if it doesn't exist
    try {
        if (!fs::exists(output_dir)) {
            fs::create_directories(output_dir);
            std::cout << "Created output directory: " << output_dir << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to create output directory: " << e.what() << "\n";
        return false;
    }
    
    sf_info.samplerate = sample_rate;
    sf_info.channels = 1;
    sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    char filename[512];

    // Create mic.wav
    snprintf(filename, sizeof(filename), "%s/mic.wav", output_dir.c_str());
    mic_file = sf_open(filename, SFM_WRITE, &sf_info);
    if (!mic_file) {
        std::cerr << "Error: Failed to create " << filename << ": " << sf_strerror(nullptr) << "\n";
        return false;
    }
    std::cout << "Created: " << filename << "\n";

    // Create near.wav
    snprintf(filename, sizeof(filename), "%s/near.wav", output_dir.c_str());
    near_file = sf_open(filename, SFM_WRITE, &sf_info);
    if (!near_file) {
        std::cerr << "Error: Failed to create " << filename << ": " << sf_strerror(nullptr) << "\n";
        return false;
    }
    std::cout << "Created: " << filename << "\n";

    // Create far.wav
    snprintf(filename, sizeof(filename), "%s/far.wav", output_dir.c_str());
    far_file = sf_open(filename, SFM_WRITE, &sf_info);
    if (!far_file) {
        std::cerr << "Error: Failed to create " << filename << ": " << sf_strerror(nullptr) << "\n";
        return false;
    }
    std::cout << "Created: " << filename << "\n";

    // Create mix.wav
    snprintf(filename, sizeof(filename), "%s/mix.wav", output_dir.c_str());
    mix_file = sf_open(filename, SFM_WRITE, &sf_info);
    if (!mix_file) {
        std::cerr << "Error: Failed to create " << filename << ": " << sf_strerror(nullptr) << "\n";
        return false;
    }
    std::cout << "Created: " << filename << "\n";

    last_print_time = std::chrono::steady_clock::now();
    return true;
}

// Close all WAV files
static void close_wav_files() {
    if (mic_file) {
        sf_close(mic_file);
        mic_file = nullptr;
    }
    if (near_file) {
        sf_close(near_file);
        near_file = nullptr;
    }
    if (far_file) {
        sf_close(far_file);
        far_file = nullptr;
    }
    if (mix_file) {
        sf_close(mix_file);
        mix_file = nullptr;
    }
}
#endif

// Simple callback for processing audio frames
static void capture_postprocess_callback(
    const float* mic,      // Microphone input
    const float* near,     // Echo-cancelled near-end output
    const float* far,      // Far-end (system audio) output
    const float* mix,      // Mix of both near and far-end outputs
    size_t frameSize,      // Number of samples in one frame
    void* user_data)       // Custom user data pointer
{
    frame_count++;

#ifdef SAVE_AUDIO_FILES
    // Write audio data to WAV files
    if (mic_file && mic) {
        sf_write_float(mic_file, mic, frameSize);
    }
    if (near_file && near) {
        sf_write_float(near_file, near, frameSize);
    }
    if (far_file && far) {
        sf_write_float(far_file, far, frameSize);
    }
    if (mix_file && mix) {
        sf_write_float(mix_file, mix, frameSize);
    }

    // Print frame size every N seconds
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_print_time);
    
    if (elapsed.count() >= FRAME_PRINT_INTERVAL_SECONDS) {
        std::cout << "[" << elapsed.count() << "s] Frame Size: " << frameSize << " samples, "
                  << "Total Frames: " << frame_count << "\n";
        last_print_time = now;
    }
#else
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - callback_start_time);
    
    static std::chrono::steady_clock::time_point last_log_time = callback_start_time;
    auto log_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);
    
    if (log_elapsed.count() >= FRAME_PRINT_INTERVAL_SECONDS) {
        std::cout << "[" << elapsed.count() << "s] Frame Size: " << frameSize << " samples, "
                  << "Total Frames: " << frame_count << "\n";
        last_log_time = now;
    }
#endif
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Nvoice SDK - Simple AEC Demo\n";
    std::cout << "========================================\n\n";
    
    // Load configuration from config.json
    std::cout << "Loading configuration from config.json...\n";
    if (!ConfigManager::load("config.json")) {
        std::cerr << "Error: Failed to load config.json\n";
        return -1;
    }
    std::cout << "Configuration loaded successfully!\n\n";
    
    // Print current configuration
    ConfigManager::printConfig();
    std::cout << "\n";
    
    // Get audio configuration from ConfigManager
    const auto& audio_config = ConfigManager::getAudioConfig();
    const int SAMPLE_RATE = audio_config.sample_rate;
    const int CHANNELS = audio_config.channels;
    const int FRAME_SIZE = audio_config.frameSize();
    
    std::cout << "Audio Configuration:\n";
    std::cout << "  Sample Rate: " << SAMPLE_RATE << " Hz\n";
    std::cout << "  Channels: " << CHANNELS << "\n";
    std::cout << "  Frame Size: " << FRAME_SIZE << " samples (" << audio_config.frame_size_ms << "ms)\n\n";
    
    // Get output configuration
    const auto& output_config = ConfigManager::getOutputConfig();
    std::string output_dir = output_config.wav_dump_directory;
    bool wav_dump_enabled = output_config.wav_dump_enabled;
    
#ifdef SAVE_AUDIO_FILES
    // Initialize WAV file output if enabled
    if (wav_dump_enabled) {
        std::cout << "Initializing audio file output...\n";
        if (!init_wav_files(output_dir, SAMPLE_RATE)) {
            std::cerr << "Error: Failed to initialize WAV files\n";
            return -1;
        }
        std::cout << "Audio files will be saved to: " << output_dir << "\n\n";
    } else {
        std::cout << "WAV file output disabled in configuration\n\n";
    }
#endif
    
    // Create SDK instance
    std::cout << "Creating SDK instance...\n";
    NvoiceAiSdk sdk(SAMPLE_RATE, CHANNELS, FRAME_SIZE);
    
    // Initialize audio processing
    std::cout << "Initializing audio processing...\n";
    if (!sdk.initAudioProcessing()) {
        std::cerr << "Error: Failed to initialize audio processing\n";
#ifdef SAVE_AUDIO_FILES
        if (wav_dump_enabled) {
            close_wav_files();
        }
#endif
        return -1;
    }
    std::cout << "Audio processing initialized successfully!\n\n";
    
    // Start real-time audio processing
    std::cout << "Starting audio capture and processing...\n";
    std::cout << "Please speak into your microphone. \n";
    std::cout << "Demo will run for " << DEMO_DURATION_SECONDS << " seconds.\n";
    std::cout << "Press Ctrl+C to stop...\n\n";
    
    callback_start_time = std::chrono::steady_clock::now();
    
    if (!sdk.start(capture_postprocess_callback, nullptr)) {
        std::cerr << "Error: Failed to start SDK\n";
#ifdef SAVE_AUDIO_FILES
        if (wav_dump_enabled) {
            close_wav_files();
        }
#endif
        return -1;
    }
    
    // Run for the specified duration
    int num_iterations = (DEMO_DURATION_SECONDS * 1000) / DEMO_CHECK_INTERVAL_MS;
    for (int i = 0; i < num_iterations; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DEMO_CHECK_INTERVAL_MS));
    }
    
    // Shutdown
    std::cout << "\nStopping audio processing...\n";
    sdk.stop();
    
#ifdef SAVE_AUDIO_FILES
    if (wav_dump_enabled) {
        std::cout << "Closing audio files...\n";
        close_wav_files();
        std::cout << "Audio files saved to: " << output_dir << "\n";
        std::cout << "  - mic.wav (microphone input)\n";
        std::cout << "  - near.wav (echo-cancelled output)\n";
        std::cout << "  - far.wav (far-end/speaker reference)\n";
        std::cout << "  - mix.wav (mixed output)\n";
    }
#endif
    
    std::cout << "Demo completed.\n";
    
    return 0;
}
