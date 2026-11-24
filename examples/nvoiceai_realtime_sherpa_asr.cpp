#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

#include "NvoiceAiSdk.h"
#include "ConfigLoader.h"
#include "SherpaOnnxManager.h"
#include "common.h"

// Global state for ASR
static std::string last_text;
static std::string last_printed_text;  // Track last printed text to avoid reprints
static int32_t segment_index = 0;
static std::mutex asr_mutex;

// Speech recognition callback - integrates ASR into audio processing pipeline
static void capture_postprocess_callback(
    const float* mic,
    const float* nearend,
    const float* farend,
    const float* mix,
    size_t frames,
    void* user_data) {
    
    if (!user_data) return;
    
    SherpaOnnxManager* asr_manager = reinterpret_cast<SherpaOnnxManager*>(user_data);
    
    // Accept the processed near-end audio for speech recognition
    asr_manager->AcceptWaveform(nearend, static_cast<int32_t>(frames));
    asr_manager->DecodeUntilReady();
    
    // Get current recognition result
    std::string text = asr_manager->GetResultText();
    
    // Track the latest text, but don't print intermediate results
    if (!text.empty() && last_text != text) {
        last_text = text;  // Update tracking without printing
    }
    
    // Check for endpoint (end of speech)
    if (asr_manager->IsEndpoint()) {
        // Only print the final result when speech ends (avoid printing intermediate partial results)
        if (!text.empty() && text != last_printed_text) {
            std::lock_guard<std::mutex> lock(asr_mutex);
            last_printed_text = text;
            
            // Convert to lowercase for display
            std::string display_text = text;
            std::transform(display_text.begin(), display_text.end(), display_text.begin(), 
                          [](auto c) { return std::tolower(c); });
            
            // Display recognition result only once per segment
            asr_manager->PrintDisplay(segment_index, display_text.c_str());
            fflush(stderr);
            
            segment_index++;
        }
        last_text.clear();  // Reset for next segment
        asr_manager->ResetStream();
    }
}

int main(int argc, char* argv[]) {
    // Enable UTF-8 console output on Windows for Chinese character support
#ifdef _WIN32
    SetConsoleCP(65001);           // Set input code page to UTF-8
    SetConsoleOutputCP(65001);     // Set output code page to UTF-8
#endif

    std::cout << "========================================\n";
    std::cout << "NvoiceAI SDK Realtime Speech Recognition Demo C++\n";
    std::cout << "========================================\n\n";
    
    if (argc > 2) {
        std::cout << "Usage: " << argv[0] << " <config_file_path>\n";
        return -1;
    }
    std::string config_file_path = (argc == 2) ? argv[1] : "configs/config_sherpa_asr.json";

    // Load configuration
    std::cout << "Loading configuration from: " << config_file_path << "\n";
    if (!ConfigLoader::load(config_file_path)) {
        std::cerr << "Warning: Failed to load " << config_file_path << ", using default values\n";
    }
    ConfigLoader::printConfig();

    // Get configuration parameters
    const auto& sherpa_config = ConfigLoader::getSherpaOnnxConfig();
    bool sherpa_enabled = sherpa_config.enabled;
    
    // Initialize ASR manager if enabled
    std::unique_ptr<SherpaOnnxManager> asr_manager;
    if (sherpa_enabled) {
        asr_manager = std::make_unique<SherpaOnnxManager>();
        
        if (!asr_manager->Init(sherpa_config)) {
            std::cerr << "Error: Failed to initialize SherpaOnnx ASR\n";
            std::cerr << "Check that model files are in the correct location\n";
            std::cerr << "Model directory: " << sherpa_config.model_dir << "\n";
            return -1;
        }
        
        std::cout << "\n✓ SherpaOnnx ASR initialized successfully\n";
        std::cout << "  Model: " << sherpa_config.model_dir << "\n";
        std::cout << "  Sample Rate: " << sherpa_config.sample_rate << " Hz\n";
        std::cout << "\n";
    } else {
        std::cout << "ASR: Disabled in configuration\n\n";
    }

    // Create SDK instance using C++ API with ConfigLoader parameters
    std::cout << "Initializing NvoiceAI SDK (C++ API)...\n";
    NvoiceAiSdk sdk(config_file_path);
    sdk.setLoggingLevel(LOGGING_LEVEL);

    // Initialize audio processing (AEC/NS/Gain) from the configuration file.
    if (!sdk.initAudioProcessing(config_file_path)) {
        std::cerr << "Error: Failed to initialize audio processing\n";
        return -1;
    }
    
    // Start real-time audio processing with ASR callback
    std::cout << "Starting real-time audio processing...\n";
    
    void* callback_user_data = sherpa_enabled ? asr_manager.get() : nullptr;
    
    // Create lambda wrapper for C++ API
    NvoiceAiFrameCallback callback = [](const float* mic, const float* nearend, const float* farend, 
                                        const float* mix, size_t frames, void* user_data) {
        capture_postprocess_callback(mic, nearend, farend, mix, frames, user_data);
    };
    
    if (!sdk.start(callback, callback_user_data)) {
        std::cerr << "Error: Failed to start SDK\n";
        return -1;
    }
    
    std::cout << "✓ SDK running in real-time mode (using C++ NvoiceAiSdk API)\n";
    std::cout << "Press Ctrl+C to stop.\n";
    std::cout << "\nListening for speech input...\n";
    std::cout << "Duration: " << DEMO_DURATION_SECONDS << " seconds\n";
    std::cout << "Recognized text will appear below:\n";
    std::cout << "=====================================================\n\n";
    
    // Run for specified duration
    auto start_time = std::chrono::steady_clock::now();
    bool running = true;
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DEMO_CHECK_INTERVAL_MS));
        
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= DEMO_DURATION_SECONDS) {
            running = false;
        }
    }
    
    std::cout << "\n=====================================================\n";
    std::cout << "Demo duration reached. Stopping audio processing...\n";
    
    // Stop processing
    sdk.stop();
    
    // Print summary
    std::cout << "\n=== Processing Complete ===\n";
    if (segment_index > 0) {
        std::cout << "Recognized " << segment_index << " speech segment(s)\n";
    } else {
        std::cout << "No speech detected\n";
    }
    
    std::cout << "\nThank you for using NvoiceAI SDK!\n\n";
    
    return 0;
}
