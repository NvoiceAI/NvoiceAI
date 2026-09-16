#pragma once
#include <string>
#include "ConfigManager.h"
#include "sherpa-onnx/c-api/c-api.h"

class SherpaOnnxManager {
public:
    SherpaOnnxManager();
    ~SherpaOnnxManager();

    // Prevent copying
    SherpaOnnxManager(const SherpaOnnxManager&) = delete;
    SherpaOnnxManager& operator=(const SherpaOnnxManager&) = delete;
    
    // Allow moving with noexcept
    SherpaOnnxManager(SherpaOnnxManager&&) noexcept;

    // Initialize recognizer with ConfigManager::SherpaOnnxConfig
    bool Init(const ConfigManager::SherpaOnnxConfig& cfg);

    // Accept waveform (samples in [-1..1]) into the active stream.
    void AcceptWaveform(const float* samples, int32_t nframes);

    // Decode while stream is ready (calls Decode repeatedly).
    void DecodeUntilReady();

    // Get current text result for the stream. Returns empty string on no result.
    std::string GetResultText();

    // Check endpoint and reset stream.
    bool IsEndpoint() const;
    void ResetStream();

    // Print helper (wraps SherpaOnnxPrint if available).
    void PrintDisplay(int32_t segment_index, const char* text);

private:
    const SherpaOnnxOnlineRecognizer* recognizer_ = nullptr;
    const SherpaOnnxOnlineStream* stream_ = nullptr;
    const SherpaOnnxDisplay* display_ = nullptr;
    int sample_rate_ = 16000;  // Stored from Init call
};