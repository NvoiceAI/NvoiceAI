#pragma once
#include <cstddef>
#include <functional>

using NvoiceAiFrameCallback = std::function<void(const float* mic, const float* near, const float* far, const float* mix, size_t frameSize, void* user_data)>;

class NvoiceAiSdk {
public:
    NvoiceAiSdk(int sampleRate, int channels, int frameSize);
    ~NvoiceAiSdk();

    // Start/stop recording + processing. The SDK owns PortAudio and a processing thread.
    bool initAudioProcessing();
    bool start(NvoiceAiFrameCallback cb, void* user_data = nullptr);
    void stop();

    // Offline processing 
    bool processOffline(const float* mic, const float* playback, float* output, size_t frameSize);

    // Privacy: true to enable separate near/far outputs; false to use mixed output.
    void setPrivacy(bool privacy);

    // Runtime AEC control: Enable/disable echo cancellation during active call.
    // This allows UI to toggle AEC on/off without restarting audio processing.
    void setAecEnabled(bool enabled);
    bool isAecEnabled() const;

    int sampleRate() const;
    int channels() const;
    int frameSize() const;

    // Control loudspeaker/system playback gain (1.0 = unity)
    void setSystemGain(float gain);
    float getSystemGain() const;

    // Enable audio file saving for audio debugging (requires SAVE_AUDIO_FILES build flag)
    // Specify the directory to store the output files (e.g., "./output")
    bool enableSaveAudioFiles(const std::string& output_dir);

private:
    struct Impl;
    Impl* impl_;

    bool startRecording(const std::string& outputDeviceName);
    void stopRecording();
    bool processAudioFrame(Impl* s, float* output);
};
