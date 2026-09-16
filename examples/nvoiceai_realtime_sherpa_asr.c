// C version of NvoiceAI SDK Real-time Demo with Speech Recognition
// This demonstrates using the C API (nvoiceai_sdk_c.h) with ASR integration
// Configuration is loaded via config_loader.h (pure C parsing)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#include "nvoiceai_sdk_c.h"
#include "config_loader.h"
#include "SherpaOnnxManager.h"

// Configuration constants
#define DEMO_DURATION_SECONDS 60
#define DEMO_CHECK_INTERVAL_MS 500

// Global state for ASR
static char last_text[1024] = {0};
static int32_t segment_index = 0;

// Speech recognition callback - integrates ASR into audio processing pipeline
static void capture_postprocess_callback(
    const float* mic,
    const float* near,
    const float* far,
    const float* mix,
    size_t frames,
    void* user_data) {
    
    if (!user_data) {
        return;
    }
    
    SherpaOnnxManager* asr_manager = (SherpaOnnxManager*)user_data;
    
    // Accept the processed near-end audio for speech recognition
    asr_manager->AcceptWaveform(near, (int32_t)frames);
    asr_manager->DecodeUntilReady();
    
    // Get current recognition result (GetResultText() returns std::string, call .c_str() for C API)
    std::string text = asr_manager->GetResultText();
    const char* text_cstr = text.c_str();
    
    if (text_cstr && text_cstr[0] != '\0' && strcmp(last_text, text_cstr) != 0) {
        strncpy(last_text, text_cstr, sizeof(last_text) - 1);
        last_text[sizeof(last_text) - 1] = '\0';
        
        // Convert to lowercase for display
        char display_text[1024];
        strncpy(display_text, text_cstr, sizeof(display_text) - 1);
        display_text[sizeof(display_text) - 1] = '\0';
        
        for (char* p = display_text; *p; ++p) {
            *p = tolower((unsigned char)*p);
        }
        
        // Display recognition result
        asr_manager->PrintDisplay(segment_index, display_text);
        fflush(stderr);
    }
    
    // Check for endpoint (end of speech)
    if (asr_manager->IsEndpoint()) {
        if (text_cstr && text_cstr[0] != '\0') {
            segment_index++;
        }
        asr_manager->ResetStream();
    }
}

int main(void) {
    printf("========================================\n");
    printf("NvoiceAI SDK Realtime Speech Recognition Demo (C API)\n");
    printf("========================================\n\n");
    
    // Load configuration using pure C config_loader
    printf("Loading configuration from config.json...\n");
    Config config;
    if (!load_config("config.json", &config)) {
        fprintf(stderr, "Error: Failed to load config.json\n");
        return -1;
    }
    print_config(&config);

    /* Get audio configuration values */
    const int SAMPLE_RATE = config.audio.sample_rate;
    const int CHANNELS = config.audio.channels;
    const int FRAME_SIZE = (SAMPLE_RATE / 1000) * config.audio.frame_size_ms;
    
    printf("Audio Configuration:\n");
    printf("  Sample Rate: %d Hz\n", SAMPLE_RATE);
    printf("  Channels: %d\n", CHANNELS);
    printf("  Frame Size: %d samples (%d ms)\n\n", FRAME_SIZE, config.audio.frame_size_ms);
    
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
        ConfigManager::SherpaOnnxConfig sherpa_cpp_config;
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
        printf("  Sample Rate: %d Hz\n", config.audio.sample_rate);
        printf("\n");
    } else {
        printf("ASR: Disabled in configuration\n\n");
    }
    
    // Create SDK instance using C API
    printf("Initializing NvoiceAI SDK (C API)...\n");  
    nvoiceai_handle_t sdk = nvoiceai_sdk_create(SAMPLE_RATE, CHANNELS, FRAME_SIZE);
    if (!sdk) {
        fprintf(stderr, "Error: Failed to create SDK instance\n");
        if (asr_manager) delete (SherpaOnnxManager*) asr_manager;
        return -1;
    }
    
    // Initialize audio processing
    if (nvoiceai_sdk_init_audio_processing(sdk) != 0) {
        fprintf(stderr, "Error: Failed to initialize audio processing\n");
        if (asr_manager) delete (SherpaOnnxManager*) asr_manager;
        nvoiceai_sdk_destroy(sdk);
        return -1;
    }
    
    // Apply AEC configuration
    nvoiceai_sdk_set_aec_enabled(sdk, config.aec.enabled);
    printf("AEC: %s\n", config.aec.enabled ? "enabled" : "disabled");
    printf("Mobile Mode: %s\n\n", config.aec.mobile_mode ? "enabled" : "disabled");
    
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
        usleep(DEMO_CHECK_INTERVAL_MS * 1000);  // Convert ms to microseconds
        
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
