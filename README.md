# Welcome to the NvoiceAI SDK

Welcome to the NvoiceAI SDK — a production-ready audio processing solution designed for real-time voice AI applications. It provides the essential audio processing capabilities and seemless integration for you to build low-latency, high-quality, and reliable voice experiences.

A **real-time full-duplex audio processing stack** for macOS that seamlessly integrates with voice AI applications. Built on proven technologies for production-grade audio capture, processing, and device management.

## Overview

NvoiceAI SDK delivers a comprehensive audio processing solution designed for demanding voice applications. It combines the power of industry-standard components to provide exceptional audio quality and reliability:

- **PortAudio** - Robust real-time audio streaming for both capture and playback
- **CoreAudio** - Native macOS device management with intelligent hot-plug detection
- **WebRTC Audio Processing Module** - Industry-leading algorithms for:
  - Echo Cancellation (AEC3)
  - Noise Suppression (NS)
  - Automatic Gain Control (AGC1, AGC2)

The SDK is optimized for **macOS only** (Intel and Apple Silicon compatible) and provides both C and C++ APIs for flexible integration.

## Key Features

### 🔄 Full-Duplex Audio Processing with high quality
- Simultaneous capture and playback processing across different devices using PortAudio and Blackhole-2ch
- 10ms low-latency processing (16kHz, mono default)
- Automatic device hotplug detection and seamless switch between internal and external devices (e.g. laptop loudspeaker/mic, headset, headphone, airpod), with device-aware AEC control
- WebRTC's proven AEC3 algorithm (used in Chrome, Meet, Teams)
- Gain control with multiple AGC modes (analog, digital, fixed)

### 🔌 Easy Integration with Post-Processing Callback
The `capture_postprocess_callback` provides a powerful extension point for integrating custom processing and third-party libraries at multiple tap points in the audio stack:

**Access processed audio at any stage:**
- **Microphone raw** - Original input signal
- **Near-end** - After echo cancellation
- **Far-end** - Playback reference signal
- **Mix** - Combined microphone + processed playback
- **Final** - After all processing for LLM interaction

See `examples/nvoiceai_realtime_simple.cpp` for a working example that demonstrates accessing these audio streams and optionally saving them to WAV files for analysis.

### 📊 Flexible Configuration
All audio processing parameters are configurable via JSON:

```json
{
  "audio": {
    "sample_rate": 16000,
    "channels": 1,
    "frame_size_ms": 10
  },
  "aec_config": {
    "enabled": true,
    "mobile_mode": false,
    "high_pass_filter_enabled": true
  }
  ...
}
```

## Architecture of the audio processing stack

```
┌───────────────────────────────────────────────────────────┐
│                    Voice AI Application                   │
│              (Your Custom Processing Logic)               │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
              ┌─────────────────────────┐
              │ capture_postprocess     │
              │ _callback               │
              │ (Extension Point)       │
              └────────┬────────────────┘
                       │
        ┌──────────────┴──────────────┐
        │                             │
        ▼                             ▼
    Mic Raw                      Far-end (Playback)
    Near-end                          │
    (Post-AEC)                        │
    Mix                               │
        │                             │
        └──────────────┬──────────────┘
                       ▼
          ┌──────────────────────────┐
          │   Audio Processing       │
          │   (WebRTC Modules)       │
          │ - Echo Cancellation      │
          │ - Noise Suppression      │
          │ - Gain Control           │
          │ - VAD                    │
          └──────────────┬───────────┘
                         │
        ┌────────────────┼────────────────┐
        │                │                │
        ▼                ▼                ▼
    Microphone      Playback         Device Hotplug
    (PortAudio)     (PortAudio       (CoreAudio)
                    + Blackhole-2ch)       
```

## Getting Started

### Prerequisites

**macOS System Requirements:**
- macOS 10.15 or later
- Intel or Apple Silicon (arm64)

**Build Dependencies:**
```bash
# Install via Homebrew
brew install cmake pkg-config libsndfile portaudio blackhole-2ch 
```

### Building the SDK Examples

1. **How to build:**
```bash
cd /path/to/NvoiceSdk
chmod +x build.sh
./build.sh
# or enable saving audio files
./build.sh -DSAVE_AUDIO_FILES=ON
```

The build produces three executables:
- `nvoiceai_realtime_simple` - C++ real-time audio processing example
- `nvoiceai_realtime_simple_c` - C API version of the real-time example
- `offline_audio_processor` - Batch processing tool for recorded audio files

### Running the Real-Time Example

**C++ Version:**
```bash
./nvoiceai_realtime_simple
```

**C Version:**
```bash
./nvoiceai_realtime_simple_c
```

The examples:
- Load configuration from `config.json`
- Capture audio from your default microphone and playback devices in realtime (e.g. laptop loudspeaker and mic array)
  Note: ensure Blackhole-2ch is selected for Output device in MacOS Sound Settings before running the example. This is to ensure any playback or system audio stream is routed into the SDK
- Apply audio processing (AEC, NS and AGC)
- Process audio through the customized capture postprocess callback (empty by default)
- Optional to save the audio into WAV files from different taping point over the audio stack

### Running Offline Processing

For batch processing of recorded audio files:

```bash
./offline_audio_processor mic.wav playback.wav output.wav [config.json]
```

**Arguments:**
- `mic.wav` - Input microphone audio (WAV, 16kHz, mono)
- `playback.wav` - Input playback/far-end audio (WAV, 16kHz, mono)
- `output.wav` - Output file for processed audio
- `config.json` - (Optional) Configuration file path

This is useful for:
- Testing audio processing offline
- Tuning parameters without real-time constraints
- Batch processing of audio logs
- Evaluating output quality

## API Usage

Work In Progress ...

## Integration Examples

Work In Progress ...

## License

See LICENSE file in the repository root.

## Version

Current Version: 1.0.0

---

**Ready to integrate professional audio processing into your voice application? Start with the examples and refer to the API headers for detailed integration guidance.**
