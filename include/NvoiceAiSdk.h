#pragma once
#include <cstddef>
#include <functional>
#include <string>

using NvoiceAiFrameCallback = std::function<void(const float* mic, const float* near, const float* far, const float* mix, size_t frameSize, void* user_data)>;

class NvoiceAiSdk {
public:
    NvoiceAiSdk(int sampleRate, int channels, int frameSize);

    // Create SDK using audio format (sample_rate/channels/frame_size) loaded from config.json.
    // If configFilePath is NULL/empty, the default configuration is used.
    explicit NvoiceAiSdk(const std::string& configFilePath = "");
    ~NvoiceAiSdk();

    // Logging level control: set verbosity at runtime.
    // Levels: 0=trace, 1=debug, 2=info, 3=warn, 4=err, 5=critical, 6=off
    // Requires spdlog library (enabled by default in production builds)
    void setLoggingLevel(int level);

    // Initialize audio processing (AEC/NS/Gain) using settings loaded from config.json.
    // If configFilePath is NULL/empty, the default configuration is used.
    // IMPORTANT: MUST be called successfully before start().
    bool initAudioProcessing(const std::string& configFilePath = "");
    bool start(NvoiceAiFrameCallback cb, void* user_data = nullptr);
    void stop();

    // Offline processing 
    bool processOffline(const float* mic, const float* playback, float* output, size_t frameSize);

    // Runtime AEC control: Enable/disable echo cancellation during active call.
    // This allows UI to toggle AEC on/off without restarting audio processing.
    void setAecEnabled(bool enabled);
    bool isAecEnabled() const;

    // Runtime NS control: Enable/disable noise suppression during active call.
    // This allows UI to toggle NS on/off without restarting audio processing.
    void setNsEnabled(bool enabled);
    bool isNsEnabled() const;

    // Runtime NS level control (1=kLow, 2=kModerate, 3=kHigh, 4=kVeryHigh).
    // Allows UI to adjust suppression aggressiveness during active call.
    // Level is clamped to [1, 4]. Only applies if NS is enabled.
    void setNsLevel(int level);
    int getNsLevel() const;

    // Runtime RNNoise enable/disable control.
    // This allows UI to toggle RNNoise on/off without restarting audio processing.
    void setRNNoiseEnabled(bool enabled);
    bool isRNNoiseEnabled() const;

    // Runtime RNNoise blend factor control (0.0 to 1.0).
    // 1.0 = fully processed (RNNoise output), 0.0 = fully original (bypass).
    void setRNNoiseBlendFactor(float factor);
    float getRNNoiseBlendFactor() const;

    int sampleRate() const;
    int channels() const;
    int frameSize() const;

    // Control system playback gain from C API (1.0 = unity)
    void setSystemGain(float gain);
    float getSystemGain() const;

private:
    struct Impl;
    Impl* impl_;

    bool startRecording(const std::string& outputDeviceName);
    void stopRecording();
    bool processAudioFrame(Impl* s, float* output);
};
