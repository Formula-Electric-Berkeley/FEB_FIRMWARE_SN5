@echo off
setlocal enabledelayedexpansion

:: CAN Library Generation Script for STM32 BMS Project (Windows)
:: Usage: generate_can.bat [OPTIONS]

:: Get script directory and BMS directory
set "SCRIPT_DIR=%~dp0"
set "BMS_DIR=%SCRIPT_DIR%.."
set "CAN_DIR=%BMS_DIR%\FEB_CAN_Library_SN4"

:: Initialize flags
set "DRY_RUN=false"
set "QUIET=false"

:: Parse command line arguments
:parse_args
if "%1"=="" goto :check_requirements

if "%1"=="--dry-run" (
    set "DRY_RUN=true"
    shift
    goto :parse_args
)
if "%1"=="-q" (
    set "QUIET=true"
    shift
    goto :parse_args
)
if "%1"=="--quiet" (
    set "QUIET=true"
    shift
    goto :parse_args
)
if "%1"=="-h" goto :show_help
if "%1"=="--help" goto :show_help

echo Unknown option: %1
echo Use --help for usage information
exit /b 1

:show_help
echo CAN Library Generation Script for STM32 BMS Project (Windows)
echo.
echo Usage: generate_can.bat [OPTIONS]
echo.
echo DESCRIPTION:
echo   Generates C source files from Python CAN message definitions using cantools
echo.
echo PROCESS:
echo   1. Run generate.py to create DBC file from Python message definitions
echo   2. Use cantools to generate C source files from DBC file
echo.
echo OPTIONS:
echo   --dry-run        Show what would be executed without running
echo   -q, --quiet      Reduce output verbosity
echo   -h, --help       Show this help message
echo.
echo EXAMPLES:
echo   generate_can.bat           Generate CAN library files
echo   generate_can.bat --dry-run Preview generation commands
echo   generate_can.bat -q        Generate with minimal output
echo.
echo REQUIREMENTS:
echo   - Python 3 with cantools package installed
echo   - FEB_CAN_Library_SN4 submodule must be present
echo   - Optional: Conda/Miniconda with 'feb_can' environment
echo.
echo NOTES:
echo   - Generated files: gen\FEB_CAN.dbc, gen\feb_can.c, gen\feb_can.h
echo   - Run from the scripts directory
echo   - This script is automatically called during submodule updates
exit /b 0

:check_requirements
:: Change to BMS directory
cd /d "%BMS_DIR%"

echo [CAN-GEN] STM32 BMS CAN Library Generation (Windows)
echo ==========================================
echo [INFO] Working directory: %BMS_DIR%
echo [INFO] CAN library directory: %CAN_DIR%

if "%DRY_RUN%"=="true" (
    echo [INFO] DRY RUN MODE - No actual execution
)

echo.

:: Check if conda is available and try to use it
conda --version >nul 2>&1
if %errorlevel% equ 0 (
    :: Try to activate conda environment if it exists
    conda env list | findstr "feb_can" >nul 2>&1
    if %errorlevel% equ 0 (
        if "%QUIET%"=="false" (
            echo [INFO] Activating conda environment 'feb_can'
        )
        call conda activate feb_can
    ) else (
        if "%QUIET%"=="false" (
            echo [INFO] Conda found but 'feb_can' environment not found
            echo [INFO] Recommend: conda create -n feb_can python=3.8 ^&^& conda activate feb_can ^&^& pip install cantools
        )
    )
) else (
    if "%QUIET%"=="false" (
        echo [INFO] Conda not found, using system Python (this is fine)
    )
)

:: Check if Python is available
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python is not installed or not in PATH
    exit /b 1
)

:: Check if cantools is installed
python -c "import cantools" 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] cantools package is not installed in current environment
    echo [INFO] Install with: pip install cantools
    echo [INFO] Or if using conda: conda install -c conda-forge cantools
    exit /b 1
)

:: Check if CAN library directory exists
if not exist "%CAN_DIR%" (
    echo [ERROR] FEB_CAN_Library_SN4 directory not found
    echo [INFO] Expected at: %CAN_DIR%
    echo [INFO] Make sure the submodule is initialized
    exit /b 1
)

:: Check if generate.py exists
if not exist "%CAN_DIR%\generate.py" (
    echo [ERROR] generate.py not found in CAN library
    exit /b 1
)

:: Create gen directory if it doesn't exist
if not exist "%CAN_DIR%\gen" (
    if "%DRY_RUN%"=="false" (
        mkdir "%CAN_DIR%\gen"
    )
    if "%QUIET%"=="false" (
        echo [INFO] Created gen directory
    )
)

if "%QUIET%"=="false" (
    echo [CHECK] Verifying requirements... OK
    echo.
)

:: Step 1: Generate DBC file from Python message definitions
call :execute_command "python generate.py" "Generating DBC file from Python message definitions" "%CAN_DIR%"
if %errorlevel% neq 0 exit /b %errorlevel%

:: Step 2: Generate C source files from DBC file  
call :execute_command "python -m cantools generate_c_source -o gen gen/FEB_CAN.dbc" "Generating C source files from DBC file" "%CAN_DIR%"
if %errorlevel% neq 0 exit /b %errorlevel%

:: Verify generated files exist
if "%DRY_RUN%"=="false" (
    if exist "%CAN_DIR%\gen\FEB_CAN.dbc" if exist "%CAN_DIR%\gen\feb_can.c" if exist "%CAN_DIR%\gen\feb_can.h" (
        if "%QUIET%"=="false" (
            echo.
            echo [SUCCESS] Generated files:
            echo   - %CAN_DIR%\gen\FEB_CAN.dbc
            echo   - %CAN_DIR%\gen\feb_can.c
            echo   - %CAN_DIR%\gen\feb_can.h
        )
    ) else (
        echo [ERROR] Some generated files are missing
        exit /b 1
    )
)

echo.
echo ============================================
echo [SUCCESS] CAN library generation completed!
echo [INFO] Finished at: %date% %time%
echo ============================================
exit /b 0

:: Function to execute or show command
:execute_command
set "cmd=%~1"
set "description=%~2"
set "working_dir=%~3"

if "%QUIET%"=="false" (
    echo [CAN-GEN] %description%
)

if "%DRY_RUN%"=="true" (
    echo [DRY RUN] Would execute in %working_dir%: %cmd%
    goto :eof
)

if "%QUIET%"=="false" (
    echo [EXEC] %cmd%
)

:: Execute command in the specified directory
pushd "%working_dir%"
%cmd%
set "exit_code=%errorlevel%"
popd
exit /b %exit_code%

:: Shift function for argument parsing
:shift
shift
goto :eof