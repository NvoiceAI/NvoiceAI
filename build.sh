#!/bin/bash

# Build script for NvoiceAI SDK Realtime Simple Demo
# 
# Usage:
#   ./build.sh                              # Build with default options
#   ./build.sh -DSAVE_AUDIO_FILES=ON        # Enable audio file saving
#   ./build.sh -DCMAKE_BUILD_TYPE=Release   # Build in Release mode

set -e  # Exit on error

echo "=========================================="
echo "NvoiceAI SDK Realtime Simple Demo - Build Script"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
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

# Create build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${BLUE}Creating build directory...${NC}"
    mkdir -p "$BUILD_DIR"
fi

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
echo "Executable location:"
echo "  $BUILD_DIR/nvoiceai_realtime_simple"
echo ""
echo "To run the demo:"
echo "  $BUILD_DIR/nvoiceai_realtime_simple"
echo ""

