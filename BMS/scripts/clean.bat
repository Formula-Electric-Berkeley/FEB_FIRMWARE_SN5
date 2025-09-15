@echo off
setlocal enabledelayedexpansion

:: Clean Build Directories Script for STM32 BMS Project
:: Usage: clean.bat

echo [CLEAN] STM32 BMS Project
echo ==========================

:: Script is in scripts directory, need to work from BMS directory
set "SCRIPT_DIR=%~dp0"
set "BMS_DIR=%SCRIPT_DIR%.."
echo [INFO] Working in BMS directory: %BMS_DIR%
cd /d "%BMS_DIR%"

:: Clean STM32 build directory
if exist "build" (
    echo [CLEAN] Removing STM32 build directory...
    rmdir /s /q "build"
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to remove STM32 build directory
        exit /b 1
    )
    echo [CLEAN] STM32 build directory removed
) else (
    echo [CLEAN] No STM32 build directory to clean
)

:: Clean test build directory
if exist "test\build" (
    echo [CLEAN] Removing test build directory...
    rmdir /s /q "test\build"
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to remove test build directory
        exit /b 1
    )
    echo [CLEAN] Test build directory removed
) else (
    echo [CLEAN] No test build directory to clean
)

echo [SUCCESS] Clean completed successfully!

endlocal