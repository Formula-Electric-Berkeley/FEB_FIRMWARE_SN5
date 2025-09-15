#!/bin/bash

# Build STM32 Firmware Script for BMS Project
# Usage: ./build.sh

set -e  # Exit on any error

echo "[BUILD] STM32 BMS Project"
echo "========================="

# Script is in scripts directory, need to work from BMS directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BMS_DIR="$(dirname "$SCRIPT_DIR")"
echo "[INFO] Working in BMS directory: $BMS_DIR"
cd "$BMS_DIR"

# Create build directory
if [[ ! -d "build" ]]; then
    echo "[BUILD] Creating build directory..."
    mkdir -p build
fi

# Navigate to build directory
cd build

# Configure CMake
echo "[CMAKE] Configuring project..."
cmake .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug

# Build the project
echo "[BUILD] Building project..."
cmake --build . --config Debug

# Check if BMS.elf exists
if [[ ! -f "BMS.elf" ]]; then
    echo "[ERROR] BMS.elf not found after build"
    exit 1
fi

echo "[BUILD] Build completed successfully"
echo "[INFO] Generated: BMS.elf"
echo "[INFO] File size: $(du -h BMS.elf | cut -f1)"
echo "[SUCCESS] STM32 firmware build completed!"