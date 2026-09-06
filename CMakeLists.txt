cmake_minimum_required(VERSION 3.10)
project(NvoiceAiRealtimeSimple)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SAVE_AUDIO_FILES "Enable saving of audio frames to WAV files (mic, near, far, mix)" OFF)

set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SDK_INCLUDE_DIR "${SDK_ROOT}/include")
set(SDK_LIB_DIR "${SDK_ROOT}/lib")
set(WEBRTC_APM_LIB_DIR "${SDK_ROOT}/webrtc_apm_lib")

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

add_executable(nvoiceai_realtime_simple examples/nvoiceai_realtime_simple.cpp)

if(SAVE_AUDIO_FILES)
    target_compile_definitions(nvoiceai_realtime_simple PRIVATE SAVE_AUDIO_FILES=1)
    message(STATUS "SAVE_AUDIO_FILES=ON -> enabling audio WAV file output")
else()
    message(STATUS "SAVE_AUDIO_FILES=OFF -> WAV file output disabled")
endif()

target_include_directories(nvoiceai_realtime_simple PRIVATE
    ${SDK_INCLUDE_DIR}
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
