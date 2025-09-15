#!/bin/bash

# Clean Build Directories Script for STM32 BMS Project
# Usage: ./clean.sh

set -e  # Exit on any error

echo "[CLEAN] STM32 BMS Project"
echo "=========================="

# Script is in scripts directory, need to work from BMS directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BMS_DIR="$(dirname "$SCRIPT_DIR")"
echo "[INFO] Working in BMS directory: $BMS_DIR"
cd "$BMS_DIR"

# Clean STM32 build directory
if [[ -d "build" ]]; then
    echo "[CLEAN] Removing STM32 build directory..."
    rm -rf build
    echo "[CLEAN] STM32 build directory removed"
else
    echo "[CLEAN] No STM32 build directory to clean"
fi

# Clean test build directory
if [[ -d "test/build" ]]; then
    echo "[CLEAN] Removing test build directory..."
    rm -rf test/build
    echo "[CLEAN] Test build directory removed"
else
    echo "[CLEAN] No test build directory to clean"
fi

echo "[SUCCESS] Clean completed successfully!"