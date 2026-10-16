# Welcome to the NvoiceAI SDK

The NvoiceAI SDK is a real-time, full-duplex audio processing solution designed to integrate seamlessly with voice AI applications. Built on proven technologies, it provides the essential audio processing capabilities developers need to deliver low-latency, high-quality, and production-ready voice experiences.

Designed as a dedicated audio front end for voice applications, the SDK offers a simple, developer-friendly API that makes integration straightforward. With just a few lines of C/C++ code, you can bring robust, real-time audio processing into your application.

The SDK currently supports macOS on both Intel and Apple Silicon, with support for Windows, Linux, and Android planned for future releases. Beyond desktop applications, as demonstrated in this project, the SDK’s modular architecture makes it easy to extend to a wide range of use cases, including mobile devices, wearables, automotive systems, and robotics. 

## Key Features

### 🔄 Full-Duplex high-quality audio processing built from WebRTC
The SDK comes with the libraries at **lib/** folder needed for the implementation that include the built-in audio processing pipeline witht the following features:
- **Low-latency Wideband and Super Wideband Mono** Supports real-time audio capture and playback with a 10 ms processing latency over both 16 kHz or 48 kHz mono input. Multi-channel audio capture with microphone array configuration is planned in the future release.
- **Seamless Audio Device Switching** For desktop applications, automatically adapts to changes among built-in audio devices and external peripherals, including microphones, speakers, headsets, headphones, and AirPods, maintaining a reliable, high-quality audio stream for downstream processing.
- **Industry-Proven DSP-based Audio Enhancement** Leverages WebRTC’s DSP-based acoustic echo cancellation (AEC), noise suppression (NS), and automatic gain control (AGC) to preserve voice quality under challenging device and acoustic conditions, including echo-path changes, background noise, reverberation and leveling issues.

### 🔌 Easy Customization with postprocessing callback
The `capture_postprocess_callback`provides a flexible extension point or a plug-in module for integrating custom audio processing and third-party libraries into the SDK’s audio pipeline.

The SDK provides accesses to the audio streams at 5 stages of the pipeline:

**Microphone Raw** — Unprocessed audio directly captured from the microphone.

**Near-End** — Microphone audio after acoustic echo cancellation (AEC), noise suppression (NS), and automatic gain control (AGC).

**Far-End** — Playback audio from sources such as system audio, voice-agent TTS playback, or VoIP calls.

**Mix** — A combined audio stream containing both near-end and far-end signals, available for applications that need to process or analyze both sources together.

**Final** — The near-end audio after additional custom post-processing through `capture_postprocess_callback`. This extension point allows you to implement a wide range of applications, for example, wake-word detection and speech recognition, ML-based audio enhancement, and audio recording at different pipeline stages for research and development.

Explore the demos in the **examples/** folder to see how the SDK can be used to build a variety of real-time voice applications.

### 📊 Easy Configuration through a JSON file
The parameters that confgures the library-provided or built-in audio processing pipeline are defined via JSON through the following template. Ready-to-use configuration files live in the **configs/** folder:

| File | Used by |
|------|---------|
| `configs/config.json` | Default configuration (also used by `offline_audio_processor`) |
| `configs/config_simple.json` | `nvoiceai_realtime_simple` / `nvoiceai_realtime_simple_c` |
| `configs/config_sherpa_asr.json` | `nvoiceai_realtime_sherpa_asr` / `nvoiceai_realtime_sherpa_asr_c` |
| `configs/config_cactus_needle.json` | `nvoiceai_realtime_cactus_needle_asr` |


```json
{
  "audio": {
    "sample_rate": 16000,
    "channels": 1,
    "frame_size_ms": 10,
    "description": "[Built-in] Basic audio configurations to initialize the SDK object. 16k for wideband and 48k for super wideband voice. If channels > 1, down mix into mono."
  },
  "aec": {
    "enabled": true,
    "mobile_mode": false,
    "high_pass_filter_enabled": true,
    "description": "[Built-in] Echo cancellation. Enable mobile if it runs on resource-limited device"
  },
  "ns": {
    "enabled": true,
    "level": 2,
    "description": "[Built-in] Noise suppression. level: 1=low, 2=moderate, 3=high, 4=veryhigh"
  },
  "gain_control": {
    "agc1_enabled": true,
    "agc1_mode": "adaptive_analog",
    "agc2_enabled": true,
    "description": "[Built-in] Adaptive gain control. Enable agc1 and agc2 by default"
  },
  "sys_out_gain": {
    "pre_gain": 1.0,   
    "post_gain_ratio": 1.0,  
    "description": "[Built-in] System or playback audio volumn control"
  }
}

```

For customized audio processing, simply append new JSON blocks to the above template for your own implementaion by highlighting `[capture postprocessing]` in the desciption attribute to distingush from the built-in ones. For example, to enable saving audio files and speech recognition using Sherpa ONNX models, append the following 2 blocks and save the file.

```json
"output": {
    "save_audio_files_enabled": true,
    "save_audio_files_directory": "./output",
    "description": "[capture postprocessing] Specify directory to save audio recordings at different stage"
  },
  "sherpa_onnx": {
    "enabled": true,
    "model_dir": "./third_party/sherpa-onnx/models/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/",
    "sample_rate": 16000,
    "num_threads": 1,
    "enable_endpoint": true,
    "encoder": "encoder-epoch-99-avg-1.int8.onnx",
    "decoder": "decoder-epoch-99-avg-1.onnx",
    "joiner": "joiner-epoch-99-avg-1.int8.onnx",
    "tokens": "tokens.txt",
    "rule1_min_trailing_silence": 2.4,
    "rule2_min_trailing_silence": 1.2,
    "rule3_min_utterance_length": 300,
    "decoding_method": "greedy_search",
    "feature_dim": 80,
    "provider": "cpu",
    "description": "[capture postprocessing] Speech recognition configuration. encoder/decoder/joiner/tokens are relative to model_dir"
  }

```

Similarly, the streaming speech recognition demo based on the Cactus **needle** model is configured with its own block, which also moves all tunables (sample rate, chunk size, startup skip, line wrapping, language and keyword biasing) out of the source code:

```json
  "cactus_needle": {
    "enabled": true,
    "model_path": "./third_party/cactus-needle/whistle.cact",
    "sample_rate": 16000,
    "chunk_ms": 1000,
    "out_capacity": 8192,
    "skip_initial_ms": 2000,
    "sentences_per_line": 4,
    "language": "",
    "keywords": "",
    "description": "[capture postprocessing] Cactus needle streaming speech recognition. sample_rate must be 16000 mono. language/keywords empty = auto-detect / no biasing"
  }

```

## Architecture of the audio processing pipeline for MacOS as an example

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
```

The build produces 6 executables:
- `nvoiceai_realtime_simple` - C++ version of the example that saves the audio files
- `nvoiceai_realtime_simple_c` - C API version of the example that saves the audio files
- `nvoiceai_realtime_sherpa_asr` - C++ version of the example with speech recognition
- `nvoiceai_realtime_sherpa_asr_c` - C API version of the example with speech recognition
- `nvoiceai_realtime_cactus_needle_asr` - C API version of the example with Cactus needle streaming speech recognition
- `offline_audio_processor` - Batch processing tool for recorded audio files

Note: `nvoiceai_realtime_sherpa_asr*` is only built when sherpa-onnx is found under `third_party/`, and `nvoiceai_realtime_cactus_needle_asr` is only built when `third_party/cactus-needle/macos-arm64` (containing `needle.h` and `libneedle.a`) is present.

### Running the example executables

Every realtime example takes a single optional argument: the path to the configuration file. When omitted, the example falls back to its own default file under `configs/`.

```bash
  ./build/nvoiceai_realtime_simple [configs/config_simple.json]
  ./build/nvoiceai_realtime_simple_c [configs/config_simple.json]
  ./build/nvoiceai_realtime_sherpa_asr [configs/config_sherpa_asr.json]
  ./build/nvoiceai_realtime_sherpa_asr_c [configs/config_sherpa_asr.json]
  ./build/nvoiceai_realtime_cactus_needle_asr [configs/config_cactus_needle.json]
  ./build/offline_audio_processor mic.wav playback.wav output.wav [configs/config.json]
```

The examples:
- Load configuration from the given JSON file (or their default under `configs/`), including the appended `[capture postprocessing]` blocks
- Capture audio from your default microphone and playback devices in realtime (e.g. laptop loudspeaker and mic array)
  Note: ensure Blackhole-2ch is selected for Output device in MacOS Sound Settings before running the example. This is to ensure any playback or system audio stream is routed into the SDK
- Apply audio processing (AEC, NS and AGC)
- Process audio through the customized capture postprocess callback (empty by default), as demostrated by the examples.

### Running Offline Processing

For batch processing of recorded audio files:

```bash
./build/offline_audio_processor mic.wav playback.wav output.wav [configs/config.json]
```

**Arguments:**
- `mic.wav` - Input microphone audio (WAV, 16kHz or 48kHz, mono)
- `playback.wav` - Input playback/far-end audio (WAV, 16kHz, mono)
- `output.wav` - Output file for processed audio
- `configs/config.json` - (Optional) Configuration file path

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
