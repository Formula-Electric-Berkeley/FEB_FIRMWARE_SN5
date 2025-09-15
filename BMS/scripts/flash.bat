@echo off
setlocal enabledelayedexpansion

:: Flash STM32 Firmware Script for BMS Project
:: Usage: flash.bat

echo [FLASH] STM32 BMS Project
echo =========================

:: Script is in scripts directory, need to work from BMS directory
set "SCRIPT_DIR=%~dp0"
set "BMS_DIR=%SCRIPT_DIR%.."
echo [INFO] Working in BMS directory: %BMS_DIR%
cd /d "%BMS_DIR%"

:: Check if BMS.elf exists
if not exist "build\BMS.elf" (
    echo [ERROR] BMS.elf not found. Run build script first.
    echo [INFO] Expected location: build\BMS.elf
    exit /b 1
)

echo [INFO] Found firmware: build\BMS.elf
for %%A in (build\BMS.elf) do (
    set "size=%%~zA"
    set /a "size_kb=!size!/1024"
    echo [INFO] File size: !size_kb!K
)

:: Check if STM32_Programmer_CLI is available
STM32_Programmer_CLI --help >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] STM32_Programmer_CLI not found in PATH
    echo [INFO] Please install STM32CubeProgrammer and ensure STM32_Programmer_CLI is in your PATH
    echo [INFO] Or manually flash the generated build\BMS.elf file
    exit /b 1
)

:: Flash the project using STM32_Programmer_CLI
echo [FLASH] Programming STM32 via SWD...
cd build

:: Attempt to flash the device
STM32_Programmer_CLI --connect port=swd --download "BMS.elf" -hardRst -rst --start
if %errorlevel% neq 0 (
    echo [ERROR] Flashing failed
    echo [INFO] Make sure:
    echo [INFO] - STM32 device is connected via SWD
    echo [INFO] - Device is not in a locked state
    echo [INFO] - Proper permissions to access the debug probe
    echo [INFO] - No other software is using the debug probe
    echo.
    echo [TIP] For development without hardware, use:
    echo [TIP] ./scripts/dev.bat --dev  (skips flashing)
    exit /b 1
)

echo [FLASH] Programming completed successfully
echo [SUCCESS] STM32 firmware flashed!

endlocal