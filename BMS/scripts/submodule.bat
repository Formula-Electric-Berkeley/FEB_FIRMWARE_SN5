@echo off
setlocal enabledelayedexpansion

:: Submodule Management Script for STM32 BMS Project (Windows)
:: Usage: submodule.bat [OPTIONS]

:: Get script directory and BMS directory
set "SCRIPT_DIR=%~dp0"
set "BMS_DIR=%SCRIPT_DIR%.."

:: Initialize flags
set "DRY_RUN=false"
set "COMMAND="

:: Function to show help
if "%1"=="-h" goto :show_help
if "%1"=="--help" goto :show_help
if "%1"=="" goto :show_help

:: Parse command line arguments
:parse_args
if "%1"=="" goto :check_command

if "%1"=="-i" (
    set "COMMAND=init"
    shift
    goto :parse_args
)
if "%1"=="--init" (
    set "COMMAND=init"
    shift
    goto :parse_args
)
if "%1"=="-u" (
    set "COMMAND=update"
    shift
    goto :parse_args
)
if "%1"=="--update" (
    set "COMMAND=update"
    shift
    goto :parse_args
)
if "%1"=="-p" (
    set "COMMAND=update"
    shift
    goto :parse_args
)
if "%1"=="--pull" (
    set "COMMAND=update"
    shift
    goto :parse_args
)
if "%1"=="-y" (
    set "COMMAND=sync"
    shift
    goto :parse_args
)
if "%1"=="--sync" (
    set "COMMAND=sync"
    shift
    goto :parse_args
)
if "%1"=="-s" (
    set "COMMAND=status"
    shift
    goto :parse_args
)
if "%1"=="--status" (
    set "COMMAND=status"
    shift
    goto :parse_args
)
if "%1"=="-r" (
    set "COMMAND=reset"
    shift
    goto :parse_args
)
if "%1"=="--reset" (
    set "COMMAND=reset"
    shift
    goto :parse_args
)
if "%1"=="--dry-run" (
    set "DRY_RUN=true"
    shift
    goto :parse_args
)

echo Unknown command/option: %1
echo Use --help for usage information
exit /b 1

:show_help
echo Submodule Management Script for STM32 BMS Project (Windows)
echo.
echo Usage: submodule.bat [OPTIONS]
echo.
echo COMMANDS:
echo   -i, --init       Initialize all submodules
echo   -u, --update     Update all submodules to latest commit
echo   -p, --pull       Update submodules from remote (same as --update)
echo   -y, --sync       Sync submodule URLs with .gitmodules
echo   -s, --status     Show status of all submodules
echo   -r, --reset      Reset submodules to commit specified in main repo
echo.
echo OPTIONS:
echo   --dry-run        Show what would be executed without running
echo   -h, --help       Show this help message
echo.
echo EXAMPLES:
echo   submodule.bat -i            Initialize submodules after cloning
echo   submodule.bat --update      Update to latest CAN library
echo   submodule.bat -s            Check submodule status
echo   submodule.bat --dry-run -u  See what update would do
echo.
echo NOTES:
echo   - Run from the scripts directory
echo   - Updates will pull latest changes from FEB_CAN_Library_SN4
echo   - After updates, commit changes to main repo to lock version
exit /b 0

:check_command
if "%COMMAND%"=="" (
    echo No command specified. Use --help for usage information.
    exit /b 1
)

:: Change to BMS directory
cd /d "%BMS_DIR%"

echo [SUBMODULE] STM32 BMS Submodule Management (Windows)
echo ========================================
echo [INFO] Working directory: %BMS_DIR%
echo [INFO] Command: %COMMAND%

if "%DRY_RUN%"=="true" (
    echo [INFO] DRY RUN MODE - No actual execution
)

echo.

:: Execute commands
if "%COMMAND%"=="init" (
    call :execute_command "git submodule init" "Initializing submodules"
    call :execute_command "git submodule update" "Updating submodules to specified commits"
    goto :success
)

if "%COMMAND%"=="update" (
    call :execute_command "git submodule update --remote --merge" "Updating submodules to latest remote commits"
    
    REM Generate CAN library files after update
    echo.
    echo [INFO] Generating CAN library C/H files...
    if exist "%SCRIPT_DIR%generate_can.bat" (
        if "%DRY_RUN%"=="true" (
            echo [DRY RUN] Would execute: generate_can.bat --quiet
        ) else (
            call "%SCRIPT_DIR%generate_can.bat" --quiet
            if !errorlevel! neq 0 (
                echo [WARNING] CAN library generation failed, but submodule update completed
            ) else (
                echo [SUCCESS] CAN library files generated successfully
            )
        )
    ) else (
        echo [WARNING] generate_can.bat not found, skipping CAN library generation
    )
    
    echo.
    echo [INFO] Submodules updated to latest versions.
    echo [INFO] Remember to commit these changes to lock the new versions:
    echo        git add .gitmodules FEB_CAN_Library_SN4
    echo        git commit -m "Update FEB CAN Library submodule"
    goto :success
)

if "%COMMAND%"=="sync" (
    call :execute_command "git submodule sync" "Synchronizing submodule URLs"
    call :execute_command "git submodule update --init --recursive" "Updating after sync"
    goto :success
)

if "%COMMAND%"=="status" (
    echo [INFO] Submodule status:
    call :execute_command "git submodule status" "Checking submodule status"
    echo.
    echo [INFO] Submodule summary:
    if "%DRY_RUN%"=="false" (
        git submodule foreach "echo   %cd:~-20%: && git rev-parse --short HEAD && git describe --always --dirty"
    ) else (
        echo [DRY RUN] Would show submodule summary
    )
    goto :success
)

if "%COMMAND%"=="reset" (
    call :execute_command "git submodule update --init" "Resetting submodules to committed versions"
    goto :success
)

:success
echo.
echo ============================================
echo [SUCCESS] Submodule operation completed!
echo [INFO] Finished at: %date% %time%
echo ============================================
exit /b 0

:: Function to execute or show command
:execute_command
set "cmd=%~1"
set "description=%~2"

echo [SUBMODULE] %description%

if "%DRY_RUN%"=="true" (
    echo [DRY RUN] Would execute: %cmd%
    goto :eof
)

echo [EXEC] %cmd%
%cmd%
goto :eof

:: Shift function for argument parsing
:shift
shift
goto :eof