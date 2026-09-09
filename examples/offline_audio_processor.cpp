#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>

#include "nvoiceai_sdk_c.h"
#include "ConfigManager.h"
#include "sndfile.h"

/**
 * Read audio frames from input file
 */
bool read_audio_frames(const std::string& input_file_path, std::vector<float>& audio_data) {
    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));
    
    SNDFILE* file = sf_open(input_file_path.c_str(), SFM_READ, &sf_info);
    if (!file) {
        std::cerr << "Error: Could not open input file: " << input_file_path << "\n";
        std::cerr << "Error: " << sf_strerror(nullptr) << "\n";
        return false;
    }
    
    std::cout << "Input file info:\n";
    std::cout << "  Sample rate: " << sf_info.samplerate << " Hz\n";
    std::cout << "  Channels: " << sf_info.channels << "\n";
    std::cout << "  Frames: " << sf_info.frames << "\n";
    std::cout << "  Duration: " << (float)sf_info.frames / sf_info.samplerate << " seconds\n";
    
    // Check if the input file has the expected format
    const auto& audio_config = ConfigManager::getAudioConfig();
    if (sf_info.samplerate != audio_config.sample_rate) {
        std::cerr << "Warning: Expected sample rate " << audio_config.sample_rate << " Hz, got " 
                  << sf_info.samplerate << " Hz. Resampling may be needed.\n";
    }
    if (sf_info.channels != audio_config.channels) {
        std::cerr << "Warning: Expected " << audio_config.channels << " channel(s), got " 
                  << sf_info.channels << ". Using first channel only.\n";
    }
    
    // Read all audio data
    audio_data.resize(sf_info.frames * sf_info.channels);
    sf_count_t num_frames = sf_readf_float(file, audio_data.data(), sf_info.frames);
    
    if (num_frames != sf_info.frames) {
        std::cerr << "Warning: Read " << num_frames << " frames, expected " << sf_info.frames << "\n";
    }
    
    sf_close(file);
    return true;
}

/**
 * Write processed audio to output file
 */
bool write_audio_frames(const std::string& output_file_path, const std::vector<float>& audio_data) {
    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));
    
    const auto& audio_config = ConfigManager::getAudioConfig();
    sf_info.samplerate = audio_config.sample_rate;
    sf_info.channels = audio_config.channels;
    sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    
    SNDFILE* file = sf_open(output_file_path.c_str(), SFM_WRITE, &sf_info);
    if (!file) {
        std::cerr << "Error: Could not open output file: " << output_file_path << "\n";
        std::cerr << "Error: " << sf_strerror(nullptr) << "\n";
        return false;
    }

    sf_count_t num_frames = audio_data.size() / sf_info.channels;
    sf_count_t frames_written = sf_writef_float(file, audio_data.data(), num_frames);
    
    if (frames_written != num_frames) {
        std::cerr << "Warning: Wrote " << frames_written << " frames, expected " << num_frames << "\n";
    }
    
    std::cout << "Output file info:\n";
    std::cout << "  File: " << output_file_path << "\n";
    std::cout << "  Sample rate: " << sf_info.samplerate << " Hz\n";
    std::cout << "  Channels: " << sf_info.channels << "\n";
    std::cout << "  Frames written: " << frames_written << "\n";
    std::cout << "  Duration: " << (float)frames_written / audio_config.sample_rate << " seconds\n";
    
    sf_close(file);
    return true;
}

void print_usage(const char* program_name) {
    std::cout << "NvoiceAI SDK - Offline Audio Processor\n\n";
    std::cout << "Usage: " << program_name << " <mic.wav> <playback.wav> <output.wav> [config.json]\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  mic.wav           - Input microphone audio file (WAV format, 16kHz, mono required)\n";
    std::cout << "  playback.wav      - Input playback audio file (WAV format, 16kHz, mono required)\n";
    std::cout << "  output.wav        - Output processed audio file (WAV format)\n";
    std::cout << "  config.json       - Optional: Configuration file (default: config.json in current directory)\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << program_name << " mic.wav playback.wav output.wav\n";
    std::cout << "  " << program_name << " mic.wav playback.wav output.wav config.json\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        print_usage(argv[0]);
        return 1;
    }

    std::string mic_file = argv[1];
    std::string playback_file = argv[2];
    std::string output_file = argv[3];
    std::string config_file = "config.json";
    
    // Parse optional config file argument
    if (argc == 5) {
        config_file = argv[4];
    }
    
    std::cout << "\n=== NvoiceAI SDK Offline Audio Processor ===\n\n";
    
    // Load configuration from JSON file
    std::cout << "Loading configuration from " << config_file << "...\n";
    if (!ConfigManager::load(config_file)) {
        std::cerr << "Warning: Could not load " << config_file << ", using default values\n";
    }
    ConfigManager::printConfig();
    
    std::cout << "\nInput files:\n";
    std::cout << "  Mic file: " << mic_file << "\n";
    std::cout << "  Playback file: " << playback_file << "\n";
    std::cout << "  Output file: " << output_file << "\n";

    // Read input audio files
    std::vector<float> mic_audio;
    std::vector<float> playback_audio;
    
    std::cout << "\nReading input audio files...\n";
    if (!read_audio_frames(mic_file, mic_audio)) {
        return 1;
    }
    std::cout << "\n";

    std::cout << "Reading playback audio file...\n";
    if (!read_audio_frames(playback_file, playback_audio)) {
        return 1;
    }
    std::cout << "\n";

    // Validate audio lengths
    if (mic_audio.size() != playback_audio.size()) {
        std::cerr << "Error: Input audio files must have the same length\n";
        std::cerr << "  Mic audio size: " << mic_audio.size() << " samples\n";
        std::cerr << "  Playback audio size: " << playback_audio.size() << " samples\n";
        return 1;
    }

    // Create SDK instance
    std::cout << "Initializing NvoiceAI SDK...\n";
    const auto& audio_config = ConfigManager::getAudioConfig();
    const int SAMPLE_RATE = audio_config.sample_rate;
    const int CHANNELS = audio_config.channels;
    const int FRAME_SIZE = audio_config.frameSize();
    nvoiceai_handle_t sdk = nvoiceai_sdk_create(SAMPLE_RATE, CHANNELS, FRAME_SIZE);
    if (!sdk) {
        std::cerr << "Error: Failed to create SDK instance\n";
        return 1;
    }

    // Initialize audio processing
    if (nvoiceai_sdk_init_audio_processing(sdk) != 0) {
        std::cerr << "Error: Failed to initialize audio processing\n";
        nvoiceai_sdk_destroy(sdk);
        return 1;
    }
    
    // Apply configuration parameters
    bool aec_enabled = ConfigManager::AecConfig().enabled;
    nvoiceai_sdk_set_aec_enabled(sdk, aec_enabled ? 1 : 0);
    
    // Set pre-gain if different from default (1.0)
    float pre_gain = ConfigManager::SysOutGainConfig().pre_gain;
    if (pre_gain != 1.0f) {
        nvoiceai_sdk_set_system_gain(sdk, pre_gain);
    }
    
    std::cout << "SDK initialized successfully\n\n";

    // Process audio frames
    std::vector<float> output_audio;
    size_t num_frames = mic_audio.size() / CHANNELS;
    
    std::cout << "Processing " << num_frames << " frames (" 
              << (float)num_frames / SAMPLE_RATE << " seconds)...\n";

    output_audio.resize(num_frames * CHANNELS);
    size_t frames_processed = 0;

    for (size_t i = 0; i < num_frames; i += FRAME_SIZE) {
        size_t remaining = num_frames - i;
        size_t frames_to_process = (remaining < FRAME_SIZE) ? remaining : FRAME_SIZE;

        // Process frame
        if (nvoiceai_sdk_process_offline(
            sdk,
            mic_audio.data() + i * CHANNELS,
            playback_audio.data() + i * CHANNELS,
            output_audio.data() + i * CHANNELS,
            frames_to_process) != 0) {
            std::cerr << "Error: Failed to process frame " << i / FRAME_SIZE << "\n";
            nvoiceai_sdk_destroy(sdk);
            return 1;
        }

        frames_processed += frames_to_process;
        
        // Progress indicator every 5 seconds
        if (frames_processed % (SAMPLE_RATE * 5) == 0) {
            float progress = (float)frames_processed / num_frames * 100.0f;
            std::cout << "  Processed: " << progress << "% (" << frames_processed
                      << "/" << num_frames << " frames, " << (float)frames_processed / SAMPLE_RATE << "s)\n";
        }
    }
    std::cout << "Processing complete: " << frames_processed << " frames processed\n\n";

    // Write output audio
    std::cout << "Writing output audio...\n";
    if (!write_audio_frames(output_file, output_audio)) {
        nvoiceai_sdk_destroy(sdk);
        return 1;
    }
    std::cout << "\n";

    // Cleanup
    nvoiceai_sdk_destroy(sdk);
    std::cout << "✓ Processing Complete\n";
    std::cout << "  Total output duration: " << (float)output_audio.size() / SAMPLE_RATE << " seconds\n\n";

    return 0;
}
