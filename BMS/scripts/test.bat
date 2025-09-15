@echo off
setlocal enabledelayedexpansion

:: Unit Test Script for BMS Project
:: Usage: test.bat [gtest_options...]

echo [TEST] BMS Unit Tests
echo ====================

set "SCRIPT_DIR=%~dp0"
set "BMS_DIR=%SCRIPT_DIR%.."
set "TEST_DIR=%BMS_DIR%\test"
set "BUILD_DIR=%TEST_DIR%\build"

echo [INFO] Working in BMS directory: %BMS_DIR%

:: Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake is not installed. Please install CMake first.
    exit /b 1
)

:: Check if C++ compiler is available
where cl >nul 2>&1
if %errorlevel% neq 0 (
    where g++ >nul 2>&1
    if errorlevel 1 (
        where clang++ >nul 2>&1
        if errorlevel 1 (
            echo [ERROR] No C++ compiler found. Please install Visual Studio, MinGW, or Clang.
            exit /b 1
        )
    )
)

:: Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo [BUILD] Creating test build directory...
    mkdir "%BUILD_DIR%"
)

cd /d "%BUILD_DIR%"

:: Detect available generators
where ninja >nul 2>&1
if %errorlevel% equ 0 (
    set "CMAKE_GENERATOR=-G Ninja"
    set "BUILD_TOOL=ninja"
) else (
    set "CMAKE_GENERATOR="
    set "BUILD_TOOL=cmake --build ."
)

:: Configure if needed
if not exist "Makefile" if not exist "build.ninja" if not exist "*.vcxproj" (
    echo [CMAKE] Configuring tests with generator: !CMAKE_GENERATOR!
    if defined CMAKE_GENERATOR (
        cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON !CMAKE_GENERATOR!
    ) else (
        cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    )
    if %errorlevel% neq 0 (
        echo [ERROR] CMake configuration failed
        exit /b 1
    )
)

:: Build tests
echo [BUILD] Building tests...
if "!BUILD_TOOL!"=="ninja" (
    ninja
) else (
    cmake --build . --target bms_tests --config Debug
)
if %errorlevel% neq 0 (
    echo [ERROR] Test build failed
    exit /b 1
)

echo.
echo [RUN] Running tests...
echo ----------------------

:: Run tests with specific arguments if provided
if "%~1"=="" (
    bms_tests.exe --gtest_color=yes
) else (
    bms_tests.exe --gtest_color=yes %*
)

if %errorlevel% neq 0 (
    echo [ERROR] Tests failed
    exit /b 1
)

echo.
echo [SUCCESS] All tests completed!

endlocal