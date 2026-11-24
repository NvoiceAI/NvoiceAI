// C version of NvoiceAI SDK Real-time Demo with Speech Recognition
// This demonstrates using the C API (nvoiceai_sdk_c.h) with ASR integration
// Configuration is loaded via config_loader.h (pure C parsing)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#define platform_sleep_ms(ms) Sleep(ms)
#else
#define platform_sleep_ms(ms) usleep((ms) * 1000)
#endif

#include "nvoiceai_sdk_c.h"
#include "config_loader.h"
#include "ConfigLoader.h"
#include "SherpaOnnxManager.h"
#include "common.h"

// Global state for ASR
static char last_text[1024] = {0};
static char last_printed_text[1024] = {0};  // Track last printed text to avoid reprints
static int32_t segment_index = 0;

// Speech recognition callback - integrates ASR into audio processing pipeline
static void capture_postprocess_callback(
    const float* mic,
    const float* nearend,
    const float* farend,
    const float* mix,
    size_t frames,
    void* user_data) {
    
    if (!user_data) {
        return;
    }
    
    SherpaOnnxManager* asr_manager = (SherpaOnnxManager*)user_data;
    
    // Accept the processed near-end audio for speech recognition
    asr_manager->AcceptWaveform(nearend, (int32_t)frames);
    asr_manager->DecodeUntilReady();
    
    // Get current recognition result (GetResultText() returns std::string, call .c_str() for C API)
    std::string text = asr_manager->GetResultText();
    const char* text_cstr = text.c_str();
    
    // Track the latest text, but don't print intermediate results
    if (text_cstr && text_cstr[0] != '\0' && strcmp(last_text, text_cstr) != 0) {
        strncpy(last_text, text_cstr, sizeof(last_text) - 1);
        last_text[sizeof(last_text) - 1] = '\0';
    }
    
    // Check for endpoint (end of speech)
    if (asr_manager->IsEndpoint()) {
        // Only print the final result when speech ends (avoid printing intermediate partial results)
        if (text_cstr && text_cstr[0] != '\0' && strcmp(text_cstr, last_printed_text) != 0) {
            strncpy(last_printed_text, text_cstr, sizeof(last_printed_text) - 1);
            last_printed_text[sizeof(last_printed_text) - 1] = '\0';
            
            // Convert to lowercase for display
            char display_text[1024];
            strncpy(display_text, text_cstr, sizeof(display_text) - 1);
            display_text[sizeof(display_text) - 1] = '\0';
            
            for (char* p = display_text; *p; ++p) {
                *p = tolower((unsigned char)*p);
            }
            
            // Display recognition result only once per segment
            asr_manager->PrintDisplay(segment_index, display_text);
            fflush(stderr);
            
            segment_index++;
        }
        last_text[0] = '\0';  // Reset for next segment
        asr_manager->ResetStream();
    }
}

int main(int argc, char* argv[]) {
    // Enable UTF-8 console output on Windows for Chinese character support
#ifdef _WIN32
    SetConsoleCP(65001);           // Set input code page to UTF-8
    SetConsoleOutputCP(65001);     // Set output code page to UTF-8
#endif

    printf("========================================\n");
    printf("NvoiceAI SDK Realtime Speech Recognition Demo (C API)\n");
    printf("========================================\n\n");
    
    if (argc > 2) {
        printf("Usage: %s <config_file_path>\n", argv[0]);
        return -1;
    }
    const char* config_file_path = (argc == 2) ? argv[1] : "configs/config_sherpa_asr.json";
    
    // Load configuration using pure C config_loader
    printf("Loading configuration from: %s\n", config_file_path);
    Config config;
    if (!load_config(config_file_path, &config)) {
        fprintf(stderr, "Error: Failed to load %s\n", config_file_path);
        return -1;
    }
    print_config(&config);

    // Initialize ASR manager if enabled
    void* asr_manager = NULL;
    if (config.sherpa_onnx.enabled) {
        // Create C++ SherpaOnnxManager instance
        asr_manager = (void*)new SherpaOnnxManager();
        
        if (!asr_manager) {
            fprintf(stderr, "Error: Failed to allocate SherpaOnnx ASR manager\n");
            return -1;
        }
        
        SherpaOnnxManager* manager = (SherpaOnnxManager*)asr_manager;
        
        // Convert C config struct to C++ SherpaOnnxConfig
        ConfigLoader::SherpaOnnxConfig sherpa_cpp_config;
        sherpa_cpp_config.enabled = config.sherpa_onnx.enabled;
        sherpa_cpp_config.model_dir = config.sherpa_onnx.model_dir;
        sherpa_cpp_config.sample_rate = config.sherpa_onnx.sample_rate;
        sherpa_cpp_config.num_threads = config.sherpa_onnx.num_threads;
        sherpa_cpp_config.enable_endpoint = config.sherpa_onnx.enable_endpoint;
        sherpa_cpp_config.encoder = config.sherpa_onnx.encoder;
        sherpa_cpp_config.decoder = config.sherpa_onnx.decoder;
        sherpa_cpp_config.joiner = config.sherpa_onnx.joiner;
        sherpa_cpp_config.tokens = config.sherpa_onnx.tokens;
        sherpa_cpp_config.rule1_min_trailing_silence = config.sherpa_onnx.rule1_min_trailing_silence;
        sherpa_cpp_config.rule2_min_trailing_silence = config.sherpa_onnx.rule2_min_trailing_silence;
        sherpa_cpp_config.rule3_min_utterance_length = config.sherpa_onnx.rule3_min_utterance_length;
        sherpa_cpp_config.decoding_method = config.sherpa_onnx.decoding_method;
        sherpa_cpp_config.feature_dim = config.sherpa_onnx.feature_dim;
        sherpa_cpp_config.provider = config.sherpa_onnx.provider;
        
        if (!manager->Init(sherpa_cpp_config)) {
            fprintf(stderr, "Error: Failed to initialize SherpaOnnx ASR\n");
            fprintf(stderr, "Check that model files are in the correct location\n");
            fprintf(stderr, "Model directory: %s\n", config.sherpa_onnx.model_dir);
            delete manager;
            return -1;
        }
        
        printf("\n✓ SherpaOnnx ASR initialized successfully\n");
        printf("  Model: %s\n", config.sherpa_onnx.model_dir);
        printf("  Sample Rate: %d Hz\n", config.sherpa_onnx.sample_rate);
        printf("\n");
    } else {
        printf("ASR: Disabled in configuration\n\n");
    }

    // Create SDK instance using C API
    printf("Initializing NvoiceAI SDK (C API)...\n");  
    nvoiceai_handle_t sdk = nvoiceai_sdk_create_with_config(config_file_path);
    if (!sdk) {
        fprintf(stderr, "Error: Failed to create SDK instance\n");
        if (asr_manager) delete (SherpaOnnxManager*) asr_manager;
        return -1;
    }
    nvoiceai_sdk_set_logging_level(sdk, LOGGING_LEVEL);
    
    /* Get audio configuration values */
    int sample_rate = 0;
    int channels = 0;
    int frame_size = 0;
    nvoiceai_sdk_get_audio_config(sdk, &sample_rate, &channels, &frame_size);
    struct AudioConfig audio_config = {sample_rate, channels, frame_size};
    
    printf("Audio Configuration:\n");
    printf("  Sample Rate: %d Hz\n", audio_config.sampleRate);
    printf("  Channels: %d\n", audio_config.channels);
    printf("  Frame Size: %d samples (10 ms)\n\n", audio_config.frameSize);
    
    // Initialize audio processing (AEC/NS/Gain) from the configuration file
    if (nvoiceai_sdk_init_audio_processing_with_config(sdk, config_file_path) != 0) {
        fprintf(stderr, "Error: Failed to initialize audio processing\n");
        if (asr_manager) delete (SherpaOnnxManager*) asr_manager;
        nvoiceai_sdk_destroy(sdk);
        return -1;
    }
    
    // Start real-time audio processing with ASR callback
    printf("Starting real-time audio processing...\n");
    if (nvoiceai_sdk_start(sdk, capture_postprocess_callback, asr_manager) != 0) {
        fprintf(stderr, "Error: Failed to start SDK\n");
        nvoiceai_sdk_destroy(sdk);
        if (asr_manager) delete (SherpaOnnxManager*) asr_manager;
        return -1;
    }
    
    printf("✓ SDK running in real-time mode (using C nvoiceai_sdk_c API)\n");
    printf("Press Ctrl+C to stop.\n");
    printf("\nListening for speech input...\n");
    printf("Duration: %d seconds\n", DEMO_DURATION_SECONDS);
    printf("Recognized text will appear below:\n");
    printf("=====================================================\n\n");
    
    // Run for specified duration
    time_t start_time = time(NULL);
    int running = 1;
    
    while (running) {
        platform_sleep_ms(DEMO_CHECK_INTERVAL_MS);
        
        time_t elapsed = time(NULL) - start_time;
        if (elapsed >= DEMO_DURATION_SECONDS) {
            running = 0;
        }
    }
    
    printf("\n=====================================================\n");
    printf("Demo duration reached. Stopping audio processing...\n");
    
    // Stop processing
    nvoiceai_sdk_stop(sdk);
    
    // Clean up
    nvoiceai_sdk_destroy(sdk);
    if (asr_manager) delete (SherpaOnnxManager*) asr_manager;
    
    // Print summary
    printf("\n=== Processing Complete ===\n");
    if (segment_index > 0) {
        printf("Recognized %d speech segment(s)\n", segment_index);
    } else {
        printf("No speech detected\n");
    }
    
    printf("\nThank you for using NvoiceAI SDK!\n\n");
    
    return 0;
}
