@echo off
setlocal enabledelayedexpansion

:: Build STM32 Firmware Script for BMS Project
:: Usage: build.bat

echo [BUILD] STM32 BMS Project
echo =========================

:: Script is in scripts directory, need to work from BMS directory
set "SCRIPT_DIR=%~dp0"
set "BMS_DIR=%SCRIPT_DIR%.."
echo [INFO] Working in BMS directory: %BMS_DIR%
cd /d "%BMS_DIR%"

:: Create build directory
if not exist "build" (
    echo [BUILD] Creating build directory...
    mkdir "build"
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to create build directory
        exit /b 1
    )
)

:: Navigate to build directory
cd build
if %errorlevel% neq 0 (
    echo [ERROR] Failed to change to build directory
    exit /b 1
)

:: Configure CMake
echo [CMAKE] Configuring project...
cmake .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

:: Build the project
echo [BUILD] Building project...
cmake --build . --config Debug
if %errorlevel% neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)

:: Check if BMS.elf exists
if not exist "BMS.elf" (
    echo [ERROR] BMS.elf not found after build
    exit /b 1
)

echo [BUILD] Build completed successfully
echo [INFO] Generated: BMS.elf
for %%A in (BMS.elf) do (
    set "size=%%~zA"
    set /a "size_kb=!size!/1024"
    echo [INFO] File size: !size_kb!K
)
echo [SUCCESS] STM32 firmware build completed!

endlocal