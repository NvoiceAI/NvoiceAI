#!/bin/bash

# Build script for NvoiceAI SDK
# (audio file saving, Sherpa ONNX and Cactus Needle support enabled by default
#  when their dependencies are found under third_party/)
# 
# Usage:
#   ./build.sh                                    # Build with default options
#   ./build.sh -DCMAKE_BUILD_TYPE=Release         # Build in Release mode
#   ./build.sh -DCACTUS_NEEDLE_ROOT=/path/to/cactus-needle/macos-arm64

set -e  # Exit on error

echo "=========================================="
echo "NvoiceAI SDK - Build Script"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="${SCRIPT_DIR}/build"

# Collect all command line arguments for CMake
CMAKE_ARGS=()

if [ $# -gt 0 ]; then
    CMAKE_ARGS=("$@")
    
    echo -e "${BLUE}CMake arguments:${NC}"
    for arg in "${CMAKE_ARGS[@]}"; do
        echo "  $arg"
    done
    echo ""
fi

# Remove existing build directory for clean build
if [ -d "$BUILD_DIR" ]; then
    echo -e "${BLUE}Removing existing build directory for clean build...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
echo -e "${BLUE}Creating build directory...${NC}"
mkdir -p "$BUILD_DIR"

# Change to build directory
cd "$BUILD_DIR"

# Configure CMake
echo -e "${BLUE}Configuring CMake...${NC}"
cmake "$SCRIPT_DIR" "${CMAKE_ARGS[@]}"

# Build the project
echo -e "${BLUE}Building project...${NC}"
cmake --build .

# Output results
echo ""
echo -e "${GREEN}=========================================="
echo "Build Complete!"
echo "==========================================${NC}"
echo ""
echo "Executables built:"
echo "  C++ version (with audio file saving):"
echo "    $BUILD_DIR/nvoiceai_realtime_simple"
echo ""
echo "  C version (with audio file saving):"
echo "    $BUILD_DIR/nvoiceai_realtime_simple_c"
echo ""
echo "  Offline audio processor (batch processing):"
echo "    $BUILD_DIR/offline_audio_processor"
echo ""
echo "  C++ version with Sherpa ONNX speech recognition:"
echo "    $BUILD_DIR/nvoiceai_realtime_sherpa_asr"
echo ""
echo "  C version with Sherpa ONNX speech recognition:"
echo "    $BUILD_DIR/nvoiceai_realtime_sherpa_asr_c"
echo ""
echo "  C version with Cactus Needle streaming speech recognition:"
echo "    $BUILD_DIR/nvoiceai_realtime_cactus_needle_asr"
echo ""
echo "Note: the Sherpa ONNX examples are only built when sherpa-onnx is found"
echo "      under third_party/, and the Cactus Needle example is only built when"
echo "      third_party/cactus-needle/macos-arm64 is present."
echo ""
echo "Every realtime example accepts a single optional argument: the path to the"
echo "configuration file. If omitted, each example falls back to its own default"
echo "file under configs/."
echo ""
echo "To run the C++ demo (with audio file saving):"
echo "  cd $SCRIPT_DIR"
echo "  $BUILD_DIR/nvoiceai_realtime_simple [configs/config_simple.json]"
echo ""
echo "To run the C demo (with audio file saving):"
echo "  cd $SCRIPT_DIR"
echo "  $BUILD_DIR/nvoiceai_realtime_simple_c [configs/config_simple.json]"
echo ""
echo "To run with Sherpa ONNX speech recognition (C++):"
echo "  cd $SCRIPT_DIR"
echo "  $BUILD_DIR/nvoiceai_realtime_sherpa_asr [configs/config_sherpa_asr.json]"
echo ""
echo "To run with Sherpa ONNX speech recognition (C):"
echo "  cd $SCRIPT_DIR"
echo "  $BUILD_DIR/nvoiceai_realtime_sherpa_asr_c [configs/config_sherpa_asr.json]"
echo ""
echo "To run with Cactus Needle streaming speech recognition (C):"
echo "  cd $SCRIPT_DIR"
echo "  $BUILD_DIR/nvoiceai_realtime_cactus_needle_asr [configs/config_cactus_needle.json]"
echo ""
echo "To process audio files offline:"
echo "  cd $SCRIPT_DIR"
echo "  $BUILD_DIR/offline_audio_processor mic.wav playback.wav output.wav [configs/config.json]"
echo ""
echo "For more information, see README.md"
echo ""
