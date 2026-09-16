#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <mutex>

#include "NvoiceAiSdk.h"
#include "ConfigManager.h"
#include "SherpaOnnxManager.h"

// Configuration constants
constexpr int DEMO_DURATION_SECONDS = 60;
constexpr int DEMO_CHECK_INTERVAL_MS = 500;

// Global state for ASR
static std::string last_text;
static int32_t segment_index = 0;
static std::mutex asr_mutex;

// Speech recognition callback - integrates ASR into audio processing pipeline
static void capture_postprocess_callback(
    const float* mic,
    const float* near,
    const float* far,
    const float* mix,
    size_t frames,
    void* user_data) {
    
    if (!user_data) return;
    
    SherpaOnnxManager* asr_manager = reinterpret_cast<SherpaOnnxManager*>(user_data);
    
    // Accept the processed near-end audio for speech recognition
    asr_manager->AcceptWaveform(near, static_cast<int32_t>(frames));
    asr_manager->DecodeUntilReady();
    
    // Get current recognition result
    std::string text = asr_manager->GetResultText();
    
    if (!text.empty() && last_text != text) {
        std::lock_guard<std::mutex> lock(asr_mutex);
        last_text = text;
        
        // Convert to lowercase for display
        std::string display_text = text;
        std::transform(display_text.begin(), display_text.end(), display_text.begin(), 
                      [](auto c) { return std::tolower(c); });
        
        // Display recognition result
        asr_manager->PrintDisplay(segment_index, display_text.c_str());
        fflush(stderr);
    }
    
    // Check for endpoint (end of speech)
    if (asr_manager->IsEndpoint()) {
        if (!text.empty()) {
            segment_index++;
        }
        asr_manager->ResetStream();
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "NvoiceAI SDK Realtime Speech Recognition Demo C++\n";
    std::cout << "========================================\n\n";
    
    // Load configuration
    std::cout << "Loading configuration from config.json...\n";
    if (!ConfigManager::load("config.json")) {
        std::cerr << "Warning: Failed to load config.json, using default values\n";
    }
    ConfigManager::printConfig();
    
    // Get configuration parameters
    const auto& audio_config = ConfigManager::getAudioConfig();
    const auto& aec_config = ConfigManager::getAecConfig();
    const auto& sherpa_config = ConfigManager::getSherpaOnnxConfig();
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
    
    // Create SDK instance using C++ API with ConfigManager parameters
    std::cout << "Initializing NvoiceAI SDK (C++ API)...\n";
    int frame_size = audio_config.frameSize();
    NvoiceAiSdk sdk(audio_config.sample_rate, audio_config.channels, frame_size);
    
    // Initialize audio processing
    if (!sdk.initAudioProcessing()) {
        std::cerr << "Error: Failed to initialize audio processing\n";
        return -1;
    }
    
    // Apply AEC configuration
    sdk.setAecEnabled(aec_config.enabled);
    std::cout << "AEC: " << (aec_config.enabled ? "enabled" : "disabled") << "\n";
    std::cout << "Mobile Mode: " << (aec_config.mobile_mode ? "enabled" : "disabled") << "\n\n";
    
    // Start real-time audio processing with ASR callback
    std::cout << "Starting real-time audio processing...\n";
    
    void* callback_user_data = sherpa_enabled ? asr_manager.get() : nullptr;
    
    // Create lambda wrapper for C++ API
    NvoiceAiFrameCallback callback = [](const float* mic, const float* near, const float* far, 
                                        const float* mix, size_t frames, void* user_data) {
        capture_postprocess_callback(mic, near, far, mix, frames, user_data);
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
