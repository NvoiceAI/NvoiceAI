cmake_minimum_required(VERSION 3.10)
project(NvoiceAiRealtimeSimple)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Prioritize Homebrew packages (macOS) to avoid conda/system package conflicts
if(APPLE)
    list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

# Find spdlog (required by SDK which includes production logging)
find_package(spdlog REQUIRED CONFIG)

set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SDK_INCLUDE_DIR "${SDK_ROOT}/include")
set(SDK_SRC_DIR "${SDK_ROOT}/src")

# Set SDK_LIB_DIR based on platform (must be resolved at configuration time for find_library)
if(WIN32)
    set(SDK_LIB_DIR "${SDK_ROOT}/lib/win")
elseif(APPLE)
    set(SDK_LIB_DIR "${SDK_ROOT}/lib/mac")
else()
    set(SDK_LIB_DIR "${SDK_ROOT}/lib")
endif()

set(UTILITIES_DIR "${SDK_ROOT}/utilities")
set(SHERPA_ONNX_LOCAL_ROOT "${SDK_ROOT}/third_party/sherpa-onnx/static")

if(APPLE)
    find_library(WEBRTC_APM_LIB
        NAMES webrtc-audio-processing-2.1
        PATHS "${SDK_LIB_DIR}"
        NO_DEFAULT_PATH
        REQUIRED
    )
    set(WEBRTC_APM_LINK_TARGETS "${WEBRTC_APM_LIB}")
    message(STATUS "Found WebRTC APM library (macOS): ${WEBRTC_APM_LIB}")
else()
    # On Windows, link against the webrtc-audio-processing library
    find_library(WEBRTC_APM_LIB 
        NAMES webrtc-audio-processing-2-1
        PATHS "${SDK_LIB_DIR}"
        NO_DEFAULT_PATH
        REQUIRED
    )
    set(WEBRTC_APM_LINK_TARGETS "${WEBRTC_APM_LIB}")
    message(STATUS "Found WebRTC Audio Processing (Windows): ${WEBRTC_APM_LIB}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(SNDFILE REQUIRED sndfile)

# C++ realtime example with audio file saving (enabled by default)
add_executable(nvoiceai_realtime_simple 
    examples/nvoiceai_realtime_simple.cpp 
    ${UTILITIES_DIR}/ConfigLoader.cpp
)

message(STATUS "Building nvoiceai_realtime_simple with audio WAV file output enabled")

target_include_directories(nvoiceai_realtime_simple PRIVATE
    ${SDK_INCLUDE_DIR}
    ${UTILITIES_DIR}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(nvoiceai_realtime_simple PRIVATE
    ${SDK_LIB_DIR}/$<IF:$<PLATFORM_ID:Windows>,nvoiceai_sdk.lib,libnvoiceai_sdk.a>
    ${WEBRTC_APM_LINK_TARGETS}
    ${PORTAUDIO_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    spdlog::spdlog
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
    ${SDK_LIB_DIR}/$<IF:$<PLATFORM_ID:Windows>,nvoiceai_sdk.lib,libnvoiceai_sdk.a>
    ${WEBRTC_APM_LINK_TARGETS}
    ${PORTAUDIO_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    c++
    spdlog::spdlog
)

target_link_directories(nvoiceai_realtime_simple_c PRIVATE
    ${SDK_LIB_DIR}
    ${PORTAUDIO_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

# Offline audio processor - for batch processing and testing (with audio file saving enabled by default)
add_executable(offline_audio_processor 
    examples/offline_audio_processor.cpp
    ${UTILITIES_DIR}/ConfigLoader.cpp
)

message(STATUS "Building offline_audio_processor with audio WAV file output enabled")

target_include_directories(offline_audio_processor PRIVATE
    ${SDK_INCLUDE_DIR}
    ${UTILITIES_DIR}
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(offline_audio_processor PRIVATE
    ${SDK_LIB_DIR}/$<IF:$<PLATFORM_ID:Windows>,nvoiceai_sdk.lib,libnvoiceai_sdk.a>
    ${WEBRTC_APM_LINK_TARGETS}
    ${PORTAUDIO_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    spdlog::spdlog
)

target_link_directories(offline_audio_processor PRIVATE
    ${SDK_LIB_DIR}
    ${PORTAUDIO_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

# Real-time example with Sherpa ONNX speech recognition (always enabled when dependencies available)
if(WIN32)
    # Windows: Use the downloaded shared library build
    set(SHERPA_ONNX_WIN_LIB_DIR "${SDK_ROOT}/third_party/sherpa-onnx-v2.0.1-win-x64-shared-MD-Release-lib/sherpa-onnx-v2.0.1-win-x64-shared-MD-Release-lib/lib")
    set(SHERPA_ONNX_INCLUDE_DIR "${SDK_ROOT}/third_party/sherpa-onnx/static/include")
    
    # Verify directories exist
    if(NOT EXISTS "${SHERPA_ONNX_WIN_LIB_DIR}")
        message(FATAL_ERROR "sherpa-onnx library directory not found: ${SHERPA_ONNX_WIN_LIB_DIR}")
    endif()
    if(NOT EXISTS "${SHERPA_ONNX_INCLUDE_DIR}")
        message(FATAL_ERROR "sherpa-onnx include directory not found: ${SHERPA_ONNX_INCLUDE_DIR}")
    endif()
    
    # Manually discover libraries
    find_library(SHERPA_ONNX_C_API_LIB NAMES sherpa-onnx-c-api PATHS "${SHERPA_ONNX_WIN_LIB_DIR}" NO_DEFAULT_PATH)
    find_library(SHERPA_ONNX_CXX_API_LIB NAMES sherpa-onnx-cxx-api PATHS "${SHERPA_ONNX_WIN_LIB_DIR}" NO_DEFAULT_PATH)
    find_library(ONNXRUNTIME_LIB NAMES onnxruntime PATHS "${SHERPA_ONNX_WIN_LIB_DIR}" NO_DEFAULT_PATH)
    
    if(NOT SHERPA_ONNX_C_API_LIB)
        message(FATAL_ERROR "Could not find sherpa-onnx-c-api.lib in ${SHERPA_ONNX_WIN_LIB_DIR}")
    endif()
    if(NOT SHERPA_ONNX_CXX_API_LIB)
        message(FATAL_ERROR "Could not find sherpa-onnx-cxx-api.lib in ${SHERPA_ONNX_WIN_LIB_DIR}")
    endif()
    if(NOT ONNXRUNTIME_LIB)
        message(FATAL_ERROR "Could not find onnxruntime.lib in ${SHERPA_ONNX_WIN_LIB_DIR}")
    endif()
    
    set(SHERPA_ONNX_LIBRARIES "${SHERPA_ONNX_C_API_LIB}" "${SHERPA_ONNX_CXX_API_LIB}" "${ONNXRUNTIME_LIB}")
    set(SHERPA_ONNX_INCLUDE_DIRS "${SHERPA_ONNX_INCLUDE_DIR}")
    set(SHERPA_ONNX_LIBRARY_DIRS "${SHERPA_ONNX_WIN_LIB_DIR}")
    
    message(STATUS "Found sherpa-onnx at ${SHERPA_ONNX_WIN_LIB_DIR}")
else()
    # macOS/Linux: Use pkg-config to find sherpa-onnx
    list(APPEND CMAKE_PREFIX_PATH "${SHERPA_ONNX_LOCAL_ROOT}")
    set(ENV{PKG_CONFIG_PATH} "${SHERPA_ONNX_LOCAL_ROOT}:$ENV{PKG_CONFIG_PATH}")
    
    pkg_check_modules(SHERPA_ONNX sherpa-onnx)
    
    if(NOT SHERPA_ONNX_FOUND)
        message(FATAL_ERROR "sherpa-onnx NOT found. Expected at: ${SHERPA_ONNX_LOCAL_ROOT}")
    endif()
    
    message(STATUS "Found sherpa-onnx at ${SHERPA_ONNX_PREFIX}")
endif()

# Build C++ Sherpa ASR example
add_executable(nvoiceai_realtime_sherpa_asr 
    examples/nvoiceai_realtime_sherpa_asr.cpp
    ${SDK_SRC_DIR}/SherpaOnnxManager.cpp
    ${UTILITIES_DIR}/ConfigLoader.cpp
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

target_link_libraries(nvoiceai_realtime_sherpa_asr PRIVATE
    ${SDK_LIB_DIR}/$<IF:$<PLATFORM_ID:Windows>,nvoiceai_sdk.lib,libnvoiceai_sdk.a>
    ${WEBRTC_APM_LINK_TARGETS}
    ${PORTAUDIO_LIBRARIES}
    ${SHERPA_ONNX_LIBRARIES}
    spdlog::spdlog
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

target_link_libraries(nvoiceai_realtime_sherpa_asr_c PRIVATE
    ${SDK_LIB_DIR}/$<IF:$<PLATFORM_ID:Windows>,nvoiceai_sdk.lib,libnvoiceai_sdk.a>
    ${WEBRTC_APM_LINK_TARGETS}
    ${PORTAUDIO_LIBRARIES}
    ${SHERPA_ONNX_LIBRARIES}
    spdlog::spdlog
)

message(STATUS "Configured nvoiceai_realtime_sherpa_asr_c (C-style API) with speech recognition")

# Real-time example with Cactus "needle" streaming speech recognition
# cactus-needle is vendored under third_party/
set(CACTUS_NEEDLE_ROOT "${SDK_ROOT}/third_party/cactus-needle/macos-arm64" CACHE PATH
    "Path to the cactus-needle macos-arm64 directory (containing needle.h and libneedle.a)")

if(EXISTS "${CACTUS_NEEDLE_ROOT}/needle.h" AND EXISTS "${CACTUS_NEEDLE_ROOT}/libneedle.a")
    message(STATUS "Found cactus-needle at ${CACTUS_NEEDLE_ROOT}")

    add_executable(nvoiceai_realtime_cactus_needle_asr
        examples/nvoiceai_realtime_cactus_needle_asr.c
        ${UTILITIES_DIR}/config_loader.c
    )

    target_include_directories(nvoiceai_realtime_cactus_needle_asr PRIVATE
        ${SDK_INCLUDE_DIR}
        ${UTILITIES_DIR}
        ${CACTUS_NEEDLE_ROOT}
        ${PORTAUDIO_INCLUDE_DIRS}
    )

    target_link_directories(nvoiceai_realtime_cactus_needle_asr PRIVATE
        ${SDK_LIB_DIR}
        ${CACTUS_NEEDLE_ROOT}
        ${PORTAUDIO_LIBRARY_DIRS}
    )

    target_link_libraries(nvoiceai_realtime_cactus_needle_asr PRIVATE
        ${SDK_LIB_DIR}/$<IF:$<PLATFORM_ID:Windows>,nvoiceai_sdk.lib,libnvoiceai_sdk.a>
        "${CACTUS_NEEDLE_ROOT}/libneedle.a"
        ${WEBRTC_APM_LINK_TARGETS}
        ${PORTAUDIO_LIBRARIES}
        c++
        spdlog::spdlog
    )

    message(STATUS "Configured nvoiceai_realtime_cactus_needle_asr (C API) with streaming speech recognition")

    # Full-duplex variant: transcribes near-end and far-end simultaneously
    # (one worker thread per direction) and shows both on a simple two-board
    # ANSI terminal GUI.
    add_executable(nvoiceai_realtime_cactus_needle_asr_fullduplex
        examples/nvoiceai_realtime_cactus_needle_asr_fullduplex.c
        ${UTILITIES_DIR}/config_loader.c
    )

    target_include_directories(nvoiceai_realtime_cactus_needle_asr_fullduplex PRIVATE
        ${SDK_INCLUDE_DIR}
        ${UTILITIES_DIR}
        ${CACTUS_NEEDLE_ROOT}
        ${PORTAUDIO_INCLUDE_DIRS}
    )

    target_link_directories(nvoiceai_realtime_cactus_needle_asr_fullduplex PRIVATE
        ${SDK_LIB_DIR}
        ${CACTUS_NEEDLE_ROOT}
        ${PORTAUDIO_LIBRARY_DIRS}
    )

    target_link_libraries(nvoiceai_realtime_cactus_needle_asr_fullduplex PRIVATE
        ${SDK_LIB_DIR}/$<IF:$<PLATFORM_ID:Windows>,nvoiceai_sdk.lib,libnvoiceai_sdk.a>
        "${CACTUS_NEEDLE_ROOT}/libneedle.a"
        ${WEBRTC_APM_LINK_TARGETS}
        ${PORTAUDIO_LIBRARIES}
        pthread
        c++
        spdlog::spdlog
    )

    message(STATUS "Configured nvoiceai_realtime_cactus_needle_asr_fullduplex (C API) with full-duplex speech recognition")
else()
    message(WARNING "cactus-needle NOT found at ${CACTUS_NEEDLE_ROOT}")
    message(WARNING "  Expected needle.h and libneedle.a there")
    message(WARNING "  Skipping nvoiceai_realtime_cactus_needle_asr target")
endif()

message(STATUS "Configured targets:")
message(STATUS "  - nvoiceai_realtime_simple (C++ with audio file saving)")
message(STATUS "  - nvoiceai_realtime_simple_c (C API example)")
message(STATUS "  - offline_audio_processor (batch processing)")
message(STATUS "  - nvoiceai_realtime_sherpa_asr (C++ with speech recognition)")
message(STATUS "  - nvoiceai_realtime_sherpa_asr_c (C-style with speech recognition)")
if(EXISTS "${CACTUS_NEEDLE_ROOT}/needle.h" AND EXISTS "${CACTUS_NEEDLE_ROOT}/libneedle.a")
    message(STATUS "  - nvoiceai_realtime_cactus_needle_asr (C API with streaming speech recognition)")
    message(STATUS "  - nvoiceai_realtime_cactus_needle_asr_fullduplex (C API, near+far ASR with 2-board GUI)")
endif()

