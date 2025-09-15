#!/bin/bash

# Flash STM32 Firmware Script for BMS Project
# Usage: ./flash.sh

set -e  # Exit on any error

echo "[FLASH] STM32 BMS Project"
echo "========================="

# Script is in scripts directory, need to work from BMS directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BMS_DIR="$(dirname "$SCRIPT_DIR")"
echo "[INFO] Working in BMS directory: $BMS_DIR"
cd "$BMS_DIR"

# Check if BMS.elf exists
if [[ ! -f "build/BMS.elf" ]]; then
    echo "[ERROR] BMS.elf not found. Run build script first."
    echo "[INFO] Expected location: build/BMS.elf"
    exit 1
fi

echo "[INFO] Found firmware: build/BMS.elf"
echo "[INFO] File size: $(du -h build/BMS.elf | cut -f1)"

# Check if STM32_Programmer_CLI is available
if ! command -v STM32_Programmer_CLI &> /dev/null; then
    echo "[ERROR] STM32_Programmer_CLI not found in PATH"
    echo "[INFO] Please install STM32CubeProgrammer and ensure STM32_Programmer_CLI is in your PATH"
    echo "[INFO] Or manually flash the generated build/BMS.elf file"
    exit 1
fi

# Flash the project using STM32_Programmer_CLI
echo "[FLASH] Programming STM32 via SWD..."
cd build

# Attempt to flash the device
if STM32_Programmer_CLI --connect port=swd --download "BMS.elf" -hardRst -rst --start; then
    echo "[FLASH] Programming completed successfully"
    echo "[SUCCESS] STM32 firmware flashed!"
else
    echo "[ERROR] Flashing failed"
    echo "[INFO] Make sure:"
    echo "[INFO] - STM32 device is connected via SWD"
    echo "[INFO] - Device is not in a locked state"
    echo "[INFO] - Proper permissions to access the debug probe"
    echo "[INFO] - No other software is using the debug probe"
    echo ""
    echo "[TIP] For development without hardware, use:"
    echo "[TIP] ./scripts/dev.sh --dev  (skips flashing)"
    exit 1
fi