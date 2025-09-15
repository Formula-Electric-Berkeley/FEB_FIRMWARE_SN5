@echo off
setlocal enabledelayedexpansion

:: Development Workflow Script for STM32 BMS Project
:: Usage: dev.bat [OPTIONS]

:: Script is in scripts directory, need to work from BMS directory
set "SCRIPT_DIR=%~dp0"
set "BMS_DIR=%SCRIPT_DIR%.."

:: Initialize flags
set DO_CLEAN=false
set DO_BUILD=false
set DO_FLASH=false
set DO_TEST=false
set DO_SUBMODULE=false
set DRY_RUN=false
set SHOW_HELP=false

:: Parse command line arguments
:parse_args
if "%~1"=="" goto :done_parsing

if "%~1"=="-c" (
    set DO_CLEAN=true
    goto :next_arg
)
if "%~1"=="--clean" (
    set DO_CLEAN=true
    goto :next_arg
)
if "%~1"=="-b" (
    set DO_BUILD=true
    goto :next_arg
)
if "%~1"=="--build" (
    set DO_BUILD=true
    goto :next_arg
)
if "%~1"=="-f" (
    set DO_FLASH=true
    goto :next_arg
)
if "%~1"=="--flash" (
    set DO_FLASH=true
    goto :next_arg
)
if "%~1"=="-t" (
    set DO_TEST=true
    goto :next_arg
)
if "%~1"=="--test" (
    set DO_TEST=true
    goto :next_arg
)
if "%~1"=="-s" (
    set DO_SUBMODULE=true
    goto :next_arg
)
if "%~1"=="--submodule" (
    set DO_SUBMODULE=true
    goto :next_arg
)
if "%~1"=="-a" (
    set DO_SUBMODULE=true
    set DO_CLEAN=true
    set DO_BUILD=true
    set DO_FLASH=true
    set DO_TEST=true
    goto :next_arg
)
if "%~1"=="--all" (
    set DO_SUBMODULE=true
    set DO_CLEAN=true
    set DO_BUILD=true
    set DO_FLASH=true
    set DO_TEST=true
    goto :next_arg
)
if "%~1"=="-bf" (
    set DO_BUILD=true
    set DO_FLASH=true
    goto :next_arg
)
if "%~1"=="-bt" (
    set DO_BUILD=true
    set DO_TEST=true
    goto :next_arg
)
if "%~1"=="-cf" (
    set DO_CLEAN=true
    set DO_BUILD=true
    set DO_FLASH=true
    goto :next_arg
)
if "%~1"=="-ct" (
    set DO_CLEAN=true
    set DO_BUILD=true
    set DO_TEST=true
    goto :next_arg
)
if "%~1"=="-cbt" (
    set DO_CLEAN=true
    set DO_BUILD=true
    set DO_TEST=true
    goto :next_arg
)
if "%~1"=="-cbf" (
    set DO_CLEAN=true
    set DO_BUILD=true
    set DO_FLASH=true
    goto :next_arg
)
if "%~1"=="--dev" (
    set DO_SUBMODULE=true
    set DO_CLEAN=true
    set DO_BUILD=true
    set DO_TEST=true
    goto :next_arg
)
if "%~1"=="--dry-run" (
    set DRY_RUN=true
    goto :next_arg
)
if "%~1"=="-h" (
    set SHOW_HELP=true
    goto :next_arg
)
if "%~1"=="--help" (
    set SHOW_HELP=true
    goto :next_arg
)

echo Unknown option: %~1
echo Use --help for usage information
exit /b 1

:next_arg
shift
goto :parse_args

:done_parsing

:: Show help if requested
if "%SHOW_HELP%"=="true" goto :show_help

:: Check if no options were provided
if "%DO_CLEAN%"=="false" if "%DO_BUILD%"=="false" if "%DO_FLASH%"=="false" if "%DO_TEST%"=="false" if "%DO_SUBMODULE%"=="false" (
    echo No tasks specified. Use --help for usage information.
    exit /b 1
)

:: Show execution plan
echo [DEV] STM32 BMS Development Workflow
echo ====================================
echo [INFO] Working directory: %BMS_DIR%
echo [INFO] Script directory: %SCRIPT_DIR%

if "%DRY_RUN%"=="true" (
    echo [INFO] DRY RUN MODE - No actual execution
)

echo.
echo Execution plan:
if "%DO_SUBMODULE%"=="true" echo   ✓ Update submodules
if "%DO_CLEAN%"=="true" echo   ✓ Clean build directories
if "%DO_BUILD%"=="true" echo   ✓ Build STM32 firmware
if "%DO_TEST%"=="true" echo   ✓ Run unit tests
if "%DO_FLASH%"=="true" echo   ✓ Flash firmware to device

if "%DRY_RUN%"=="false" (
    echo.
    set /p "CONFIRM=Continue? [Y/n] "
    if /i "!CONFIRM!"=="n" (
        echo Aborted by user.
        exit /b 0
    )
)

:: Execute tasks in order
set START_TIME=%time%

if "%DO_SUBMODULE%"=="true" call :execute_step "SUBMODULE" "submodule.bat --update"
if "%DO_CLEAN%"=="true" call :execute_step "CLEAN" "clean.bat"
if "%DO_BUILD%"=="true" call :execute_step "BUILD" "build.bat"
if "%DO_TEST%"=="true" call :execute_step "TEST" "test.bat"
if "%DO_FLASH%"=="true" call :execute_step "FLASH" "flash.bat"

echo.
echo ============================================
echo [SUCCESS] Development workflow completed!
echo [INFO] Finished at: %date% %time%
echo ============================================
goto :end

:execute_step
echo.
echo ============================================
echo [%~1] %time%
echo ============================================

if "%DRY_RUN%"=="true" (
    echo [DRY RUN] Would execute: %~2
    exit /b 0
)

:: Parse script name and arguments
for /f "tokens=1*" %%a in ("%~2") do (
    set "script_name=%%a"
    set "script_args=%%b"
)

if exist "%SCRIPT_DIR%\%script_name%" (
    if "%script_args%"=="" (
        REM No arguments
        call "%SCRIPT_DIR%\%script_name%"
    ) else (
        REM Has arguments
        call "%SCRIPT_DIR%\%script_name%" %script_args%
    )
    if %errorlevel% neq 0 (
        echo [ERROR] %~1 step failed
        exit /b %errorlevel%
    )
) else (
    echo [ERROR] Script not found: %script_name%
    exit /b 1
)
exit /b 0

:show_help
echo Development Workflow Script for STM32 BMS Project
echo.
echo Usage: dev.bat [OPTIONS]
echo.
echo INDIVIDUAL TASK OPTIONS:
echo   -c, --clean       Clean build directories
echo   -b, --build       Build STM32 firmware
echo   -f, --flash       Flash firmware to device
echo   -t, --test        Run unit tests
echo   -s, --submodule   Update submodules to latest
echo.
echo COMBINATION OPTIONS:
echo   -a, --all         Run all tasks in order (submodule → clean → build → test → flash)
echo   --dev             Development mode (submodule → clean → build → test, no flash)
echo   -bf               Build then flash (common workflow)
echo   -bt               Build then test
echo   -cf               Clean, build, then flash
echo   -ct               Clean, build, then test
echo   -cbt              Clean, build, then test
echo   -cbf              Clean, build, then flash
echo.
echo UTILITY OPTIONS:
echo   --dry-run         Show what would be executed without running
echo   -h, --help        Show this help message
echo.
echo EXAMPLES:
echo   dev.bat --dev           Development cycle (no hardware needed)
echo   dev.bat --all           Complete cycle (requires STM32 device)
echo   dev.bat -bf             Quick build and flash
echo   dev.bat -bt             Build and test code
echo   dev.bat --dry-run -a    See what --all would do
echo.
echo INDIVIDUAL SCRIPTS:
echo   clean.bat               Clean build directories only
echo   build.bat               Build STM32 firmware only
echo   flash.bat               Flash firmware to device only
echo   test.bat                Run unit tests only
echo   submodule.bat           Manage git submodules
echo   generate_can.bat        Generate CAN library files
exit /b 0

:end
endlocal