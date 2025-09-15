@echo off
rem Cross-platform batch script to run tests on Windows (command line equivalent to run_tests.sh)
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "TEST_DIR=%SCRIPT_DIR%..\test"
set "BUILD_DIR=%TEST_DIR%\build"

echo ================================
echo BMS Unit Test Runner (Windows)
echo ================================
echo.

rem Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo Error: CMake is not installed. Please install CMake first.
    exit /b 1
)

rem Check if C++ compiler is available
where cl >nul 2>&1
if %errorlevel% neq 0 (
    where g++ >nul 2>&1
    if errorlevel 1 (
        where clang++ >nul 2>&1
        if errorlevel 1 (
            echo Error: No C++ compiler found. Please install Visual Studio, MinGW, or Clang.
            exit /b 1
        )
    )
)

rem Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo Creating test build directory...
    mkdir "%BUILD_DIR%"
)

cd /d "%BUILD_DIR%"

rem Detect available generators
where ninja >nul 2>&1
if %errorlevel% equ 0 (
    set "CMAKE_GENERATOR=-G Ninja"
    set "BUILD_TOOL=ninja"
) else (
    set "CMAKE_GENERATOR="
    set "BUILD_TOOL=cmake --build ."
)

rem Configure if needed
if not exist "Makefile" if not exist "build.ninja" if not exist "*.vcxproj" (
    echo Configuring tests with generator: !CMAKE_GENERATOR!
    if defined CMAKE_GENERATOR (
        cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON !CMAKE_GENERATOR!
    ) else (
        cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    )
    if %errorlevel% neq 0 (
        echo Error: CMake configuration failed
        exit /b 1
    )
)

rem Build tests
echo Building tests...
if "!BUILD_TOOL!"=="ninja" (
    ninja
    if %errorlevel% neq 0 (
        echo Error: Build failed
        exit /b 1
    )
) else (
    cmake --build . --target bms_tests --config Debug
    if %errorlevel% neq 0 (
        echo Error: Build failed
        exit /b 1
    )
)

echo.
echo Running tests...
echo --------------------------------

rem Run tests with specific arguments if provided
if "%~1"=="" (
    bms_tests.exe --gtest_color=yes
) else (
    bms_tests.exe --gtest_color=yes %*
)

if %errorlevel% neq 0 (
    echo Error: Tests failed
    exit /b 1
)

echo.
echo ================================
echo All tests completed!
echo ================================