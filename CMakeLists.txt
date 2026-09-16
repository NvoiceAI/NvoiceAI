cmake_minimum_required(VERSION 3.10)
project(NvoiceAiRealtimeSimple)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SDK_INCLUDE_DIR "${SDK_ROOT}/include")
set(SDK_SRC_DIR "${SDK_ROOT}/src")
set(SDK_LIB_DIR "${SDK_ROOT}/lib")
set(UTILITIES_DIR "${SDK_ROOT}/utilities")
set(WEBRTC_APM_LIB_DIR "${SDK_ROOT}/webrtc_apm_lib")
set(SHERPA_ONNX_LOCAL_ROOT "${SDK_ROOT}/third_party/sherpa-onnx/static")

find_library(WEBRTC_APM_LIB
    NAMES webrtc-apm
    PATHS "${WEBRTC_APM_LIB_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
)

message(STATUS "Found WebRTC APM library: ${WEBRTC_APM_LIB}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(SNDFILE REQUIRED sndfile)

# C++ realtime example with audio file saving (enabled by default)
add_executable(nvoiceai_realtime_simple examples/nvoiceai_realtime_simple.cpp)

message(STATUS "Building nvoiceai_realtime_simple with audio WAV file output enabled")

target_include_directories(nvoiceai_realtime_simple PRIVATE
    ${SDK_INCLUDE_DIR}
    ${UTILITIES_DIR}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(nvoiceai_realtime_simple PRIVATE
    "${SDK_LIB_DIR}/libnvoiceai_sdk.a"
    ${WEBRTC_APM_LIB}
    ${PORTAUDIO_LIBRARIES}
    ${SNDFILE_LIBRARIES}
)

target_link_directories(nvoiceai_realtime_simple PRIVATE
    ${SDK_LIB_DIR}
    ${PORTAUDIO_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

# C realtime example with audio file saving (enabled by default)
add_executable(nvoiceai_realtime_simple_c 
    examples/nvoiceai_realtime_simple.c
    ${UTILITIES_DIR}/config_loader.c
)

message(STATUS "Building nvoiceai_realtime_simple_c with audio WAV file output enabled")

target_include_directories(nvoiceai_realtime_simple_c PRIVATE
    ${SDK_INCLUDE_DIR}
    ${UTILITIES_DIR}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(nvoiceai_realtime_simple_c PRIVATE
    "${SDK_LIB_DIR}/libnvoiceai_sdk.a"
    ${WEBRTC_APM_LIB}
    ${PORTAUDIO_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    c++
)

target_link_directories(nvoiceai_realtime_simple_c PRIVATE
    ${SDK_LIB_DIR}
    ${PORTAUDIO_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

# Offline audio processor - for batch processing and testing (with audio file saving enabled by default)
add_executable(offline_audio_processor 
    examples/offline_audio_processor.cpp
)

message(STATUS "Building offline_audio_processor with audio WAV file output enabled")

target_include_directories(offline_audio_processor PRIVATE
    ${SDK_INCLUDE_DIR}
    ${UTILITIES_DIR}
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(offline_audio_processor PRIVATE
    "${SDK_LIB_DIR}/libnvoiceai_sdk.a"
    ${WEBRTC_APM_LIB}
    ${PORTAUDIO_LIBRARIES}
    ${SNDFILE_LIBRARIES}
)

target_link_directories(offline_audio_processor PRIVATE
    ${SDK_LIB_DIR}
    ${PORTAUDIO_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

# Real-time example with Sherpa ONNX speech recognition (enabled by default)
message(STATUS "Building with sherpa-onnx integration enabled")

# Set up pkg-config path for local sherpa-onnx
list(APPEND CMAKE_PREFIX_PATH "${SHERPA_ONNX_LOCAL_ROOT}")
set(ENV{PKG_CONFIG_PATH} "${SHERPA_ONNX_LOCAL_ROOT}:$ENV{PKG_CONFIG_PATH}")

# Find sherpa-onnx using pkg-config
pkg_check_modules(SHERPA_ONNX sherpa-onnx)

if(SHERPA_ONNX_FOUND)
    message(STATUS "Found sherpa-onnx at ${SHERPA_ONNX_PREFIX}")
    
    # Build C++ Sherpa ASR example
    add_executable(nvoiceai_realtime_sherpa_asr 
        examples/nvoiceai_realtime_sherpa_asr.cpp
        ${UTILITIES_DIR}/ConfigManager.cpp
        ${SDK_SRC_DIR}/SherpaOnnxManager.cpp
    )
    
    target_include_directories(nvoiceai_realtime_sherpa_asr PRIVATE
        ${SDK_INCLUDE_DIR}
        ${UTILITIES_DIR}
        ${PORTAUDIO_INCLUDE_DIRS}
        ${SHERPA_ONNX_INCLUDE_DIRS}
    )
    
    target_link_directories(nvoiceai_realtime_sherpa_asr PRIVATE
        ${SDK_LIB_DIR}
        ${PORTAUDIO_LIBRARY_DIRS}
        ${SHERPA_ONNX_LIBRARY_DIRS}
    )
    
    # Link against static library for real-time audio with PortAudio
    target_link_libraries(nvoiceai_realtime_sherpa_asr PRIVATE
        "${SDK_LIB_DIR}/libnvoiceai_sdk.a"
        ${WEBRTC_APM_LIB}
        ${PORTAUDIO_LIBRARIES}
        ${SHERPA_ONNX_LIBRARIES}
    )
    
    message(STATUS "Configured nvoiceai_realtime_sherpa_asr (C++) with speech recognition")
    
    # Build C Sherpa ASR example (uses C++ internally via SherpaOnnxManager)
    add_executable(nvoiceai_realtime_sherpa_asr_c
        examples/nvoiceai_realtime_sherpa_asr.c
        ${UTILITIES_DIR}/config_loader.c
        ${SDK_SRC_DIR}/SherpaOnnxManager.cpp
    )
    
    # Compile as C++ even though the main source is .c to allow calling C++ helper functions
    set_source_files_properties(examples/nvoiceai_realtime_sherpa_asr.c PROPERTIES LANGUAGE CXX)
    
    target_include_directories(nvoiceai_realtime_sherpa_asr_c PRIVATE
        ${UTILITIES_DIR}
        ${SDK_INCLUDE_DIR}
        ${PORTAUDIO_INCLUDE_DIRS}
        ${SHERPA_ONNX_INCLUDE_DIRS}
    )
    
    target_link_directories(nvoiceai_realtime_sherpa_asr_c PRIVATE
        ${SDK_LIB_DIR}
        ${PORTAUDIO_LIBRARY_DIRS}
        ${SHERPA_ONNX_LIBRARY_DIRS}
    )
    
    # Link against static library for real-time audio with PortAudio
    target_link_libraries(nvoiceai_realtime_sherpa_asr_c PRIVATE
        "${SDK_LIB_DIR}/libnvoiceai_sdk.a"
        ${WEBRTC_APM_LIB}
        ${PORTAUDIO_LIBRARIES}
        ${SHERPA_ONNX_LIBRARIES}
    )
    
    message(STATUS "Configured nvoiceai_realtime_sherpa_asr_c (C-style API) with speech recognition")
else()
    message(WARNING "sherpa-onnx NOT found")
    message(WARNING "  Expected at: ${SHERPA_ONNX_LOCAL_ROOT}")
    message(WARNING "  Make sure third_party/sherpa-onnx is properly built")
    message(WARNING "  Skipping nvoiceai_realtime_sherpa_asr targets")
endif()

message(STATUS "Configured targets:")
message(STATUS "  - nvoiceai_realtime_simple (C++ with audio file saving)")
message(STATUS "  - nvoiceai_realtime_simple_c (C API example)")
message(STATUS "  - offline_audio_processor (batch processing)")
if(SHERPA_ONNX_FOUND)
    message(STATUS "  - nvoiceai_realtime_sherpa_asr (C++ with speech recognition)")
    message(STATUS "  - nvoiceai_realtime_sherpa_asr_c (C-style with speech recognition)")
endif()
