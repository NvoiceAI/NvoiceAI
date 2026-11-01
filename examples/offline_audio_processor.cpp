#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>

#include "nvoiceai_sdk_c.h"
#include "ConfigLoader.h"
#include "sndfile.h"
#include "common.h"

/**
 * Read audio frames from input file
 */
bool read_audio_frames(const std::string& input_file_path, std::vector<float>& audio_data, const AudioConfig& audio_config) {
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
    if (sf_info.samplerate != audio_config.sampleRate) {
        std::cerr << "Error: Expected sample rate " << audio_config.sampleRate << " Hz, got " 
                  << sf_info.samplerate << " Hz.\n";
        return false;
    }
    if (sf_info.channels != audio_config.channels) {
        std::cerr << "Error: Expected " << audio_config.channels << " channel(s), got " 
                  << sf_info.channels << ".\n";
        return false;
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
bool write_audio_frames(const std::string& output_file_path, const std::vector<float>& audio_data, const AudioConfig& audio_config) {
    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));

    sf_info.samplerate = audio_config.sampleRate;
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
    std::cout << "  Duration: " << (float)frames_written / audio_config.sampleRate << " seconds\n";
    
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
    std::cout << "  config.json       - Optional: Configuration file (default: configs/config.json)\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << program_name << " mic.wav playback.wav output.wav\n";
    std::cout << "  " << program_name << " mic.wav playback.wav output.wav configs/config.json\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        print_usage(argv[0]);
        return 1;
    }

    std::string mic_file = argv[1];
    std::string playback_file = argv[2];
    std::string output_file = argv[3];
    std::string config_file = "configs/config.json";
    
    // Parse optional config file argument
    if (argc == 5) {
        config_file = argv[4];
    }
    
    std::cout << "\n=== NvoiceAI SDK Offline Audio Processor ===\n\n";
    
    // Load configuration from JSON file
    std::cout << "Loading configuration from " << config_file << "...\n";
    if (!ConfigLoader::load(config_file)) {
        std::cerr << "Warning: Could not load " << config_file << ", using default values\n";
    }
    ConfigLoader::printConfig();

    // Create SDK instance
    std::cout << "Initializing NvoiceAI SDK...\n";
    nvoiceai_handle_t sdk = nvoiceai_sdk_create_with_config(config_file.c_str());
    if (!sdk) {
        std::cerr << "Error: Failed to create SDK instance\n";
        return 1;
    }

    // Get audio configuration
    int sample_rate = 0;
    int channels = 0;
    int frame_size = 0;
    nvoiceai_sdk_get_audio_config(sdk, &sample_rate, &channels, &frame_size);
    AudioConfig audio_config{sample_rate, channels, frame_size};

    // Initialize audio processing (AEC/NS/Gain) from config.json
    if (nvoiceai_sdk_init_audio_processing_with_config(sdk, config_file.c_str()) != 0) {
        std::cerr << "Error: Failed to initialize audio processing\n";
        nvoiceai_sdk_destroy(sdk);
        return 1;
    }
    std::cout << "SDK initialized successfully\n\n";

    std::cout << "\nInput files:\n";
    std::cout << "  Mic file: " << mic_file << "\n";
    std::cout << "  Playback file: " << playback_file << "\n";
    std::cout << "  Output file: " << output_file << "\n";

    // Read input audio files
    std::vector<float> mic_audio;
    std::vector<float> playback_audio;
    
    std::cout << "\nReading input audio files...\n";
    if (!read_audio_frames(mic_file, mic_audio, audio_config)) {
        return 1;
    }
    std::cout << "\n";

    std::cout << "Reading playback audio file...\n";
    if (!read_audio_frames(playback_file, playback_audio, audio_config)) {
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

    // Process audio frames
    std::vector<float> output_audio;
    size_t num_frames = mic_audio.size() / audio_config.channels;
    
    std::cout << "Processing " << num_frames << " frames (" 
              << (float)num_frames / audio_config.sampleRate << " seconds)...\n";

    output_audio.resize(num_frames * audio_config.channels);
    size_t frames_processed = 0;

    for (size_t i = 0; i < num_frames; i += audio_config.frameSize) {
        size_t remaining = num_frames - i;
        size_t frames_to_process = (remaining < audio_config.frameSize) ? remaining : audio_config.frameSize;

        // Process frame
        if (nvoiceai_sdk_process_offline(
            sdk,
            mic_audio.data() + i * audio_config.channels,
            playback_audio.data() + i * audio_config.channels,
            output_audio.data() + i * audio_config.channels,
            frames_to_process) != 0) {
            std::cerr << "Error: Failed to process frame " << i / audio_config.frameSize << "\n";
            nvoiceai_sdk_destroy(sdk);
            return 1;
        }

        frames_processed += frames_to_process;
        
        // Progress indicator every 5 seconds
        if (frames_processed % (audio_config.sampleRate * 5) == 0) {
            float progress = (float)frames_processed / num_frames * 100.0f;
            std::cout << "  Processed: " << progress << "% (" << frames_processed
                      << "/" << num_frames << " frames, " << (float)frames_processed / audio_config.sampleRate << "s)\n";
        }
    }
    std::cout << "Processing complete: " << frames_processed << " frames processed\n\n";

    // Write output audio
    std::cout << "Writing output audio...\n";
    if (!write_audio_frames(output_file, output_audio, audio_config)) {
        nvoiceai_sdk_destroy(sdk);
        return 1;
    }
    std::cout << "\n";

    // Cleanup
    nvoiceai_sdk_destroy(sdk);
    std::cout << "✓ Processing Complete\n";
    std::cout << "  Total output duration: " << (float)output_audio.size() / audio_config.sampleRate << " seconds\n\n";

    return 0;
}
