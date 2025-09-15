#!/bin/bash

# Unit Test Script for BMS Project
# Usage: ./test.sh [gtest_options...]

set -e  # Exit on any error

echo "[TEST] BMS Unit Tests"
echo "===================="

# Detect the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BMS_DIR="$(dirname "$SCRIPT_DIR")"
TEST_DIR="$BMS_DIR/test"
BUILD_DIR="$TEST_DIR/build"

echo "[INFO] Working in BMS directory: $BMS_DIR"

# Detect operating system
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    IS_WINDOWS=true
    EXECUTABLE_EXT=".exe"
else
    IS_WINDOWS=false
    EXECUTABLE_EXT=""
fi

# Check required tools
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake is not installed. Please install CMake first."
    exit 1
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "[ERROR] No C++ compiler found. Please install g++ or clang++."
    exit 1
fi

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "[BUILD] Creating test build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Detect available generators
if command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR="-G Ninja"
    BUILD_TOOL="ninja"
elif command -v make >/dev/null 2>&1; then
    CMAKE_GENERATOR="-G \"Unix Makefiles\""
    BUILD_TOOL="make"
else
    CMAKE_GENERATOR=""
    BUILD_TOOL="cmake --build ."
fi

# Configure if needed
if [ ! -f "Makefile" ] && [ ! -f "build.ninja" ] && [ ! -f "*.vcxproj" ]; then
    echo "[CMAKE] Configuring tests with generator: ${CMAKE_GENERATOR:-default}"
    if [ -n "$CMAKE_GENERATOR" ]; then
        eval cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $CMAKE_GENERATOR
    else
        cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    fi
fi

# Build tests
echo "[BUILD] Building tests..."
if [[ "$BUILD_TOOL" == "make" ]]; then
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
elif [[ "$BUILD_TOOL" == "ninja" ]]; then
    ninja
else
    cmake --build . --target bms_tests --config Debug
fi

echo ""
echo "[RUN] Running tests..."
echo "----------------------"

TEST_EXECUTABLE="./bms_tests${EXECUTABLE_EXT}"

# Run tests with specific arguments if provided
if [ $# -eq 0 ]; then
    "$TEST_EXECUTABLE" --gtest_color=yes
else
    "$TEST_EXECUTABLE" --gtest_color=yes "$@"
fi

echo ""
echo "[SUCCESS] All tests completed!"