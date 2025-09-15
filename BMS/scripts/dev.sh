#!/bin/bash

# Development Workflow Script for STM32 BMS Project
# Usage: ./dev.sh [OPTIONS]

set -e  # Exit on any error

# Script is in scripts directory, need to work from BMS directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BMS_DIR="$(dirname "$SCRIPT_DIR")"

# Initialize flags
DO_CLEAN=false
DO_BUILD=false
DO_FLASH=false
DO_TEST=false
DO_SUBMODULE=false
DRY_RUN=false

# Function to show help
show_help() {
    echo "Development Workflow Script for STM32 BMS Project"
    echo ""
    echo "Usage: ./dev.sh [OPTIONS]"
    echo ""
    echo "INDIVIDUAL TASK OPTIONS:"
    echo "  -c, --clean       Clean build directories"
    echo "  -b, --build       Build STM32 firmware"
    echo "  -f, --flash       Flash firmware to device"
    echo "  -t, --test        Run unit tests"
    echo "  -s, --submodule   Update submodules to latest"
    echo ""
    echo "COMBINATION OPTIONS:"
    echo "  -a, --all         Run all tasks in order (submodule → clean → build → test → flash)"
    echo "  --dev             Development mode (submodule → clean → build → test, no flash)"
    echo "  -bf               Build then flash (common workflow)"
    echo "  -bt               Build then test"
    echo "  -cf               Clean, build, then flash"
    echo "  -ct               Clean, build, then test"
    echo "  -cbt              Clean, build, then test"
    echo "  -cbf              Clean, build, then flash"
    echo ""
    echo "UTILITY OPTIONS:"
    echo "  --dry-run         Show what would be executed without running"
    echo "  -h, --help        Show this help message"
    echo ""
    echo "EXAMPLES:"
    echo "  ./dev.sh --dev           Development cycle (no hardware needed)"
    echo "  ./dev.sh --all           Complete cycle (requires STM32 device)"
    echo "  ./dev.sh -bf             Quick build and flash"
    echo "  ./dev.sh -bt             Build and test code"
    echo "  ./dev.sh --dry-run -a    See what --all would do"
    echo ""
    echo "INDIVIDUAL SCRIPTS:"
    echo "  ./clean.sh               Clean build directories only"
    echo "  ./build.sh               Build STM32 firmware only"
    echo "  ./flash.sh               Flash firmware to device only"
    echo "  ./test.sh                Run unit tests only"
    echo "  ./submodule.sh           Manage git submodules"
    echo "  ./generate_can.sh        Generate CAN library files"
    exit 0
}

# Function to execute or show command
execute_step() {
    local step_name="$1"
    local script_command="$2"
    
    echo ""
    echo "============================================"
    echo "[$step_name] $(date '+%H:%M:%S')"
    echo "============================================"
    
    if [[ "$DRY_RUN" == true ]]; then
        echo "[DRY RUN] Would execute: $script_command"
        return 0
    fi
    
    # Parse script name and arguments
    local script_name=$(echo "$script_command" | awk '{print $1}')
    local script_args=$(echo "$script_command" | cut -d' ' -f2-)
    
    if [[ -f "$SCRIPT_DIR/$script_name" ]]; then
        if [[ "$script_name" == "$script_command" ]]; then
            # No arguments
            "$SCRIPT_DIR/$script_name"
        else
            # Has arguments
            "$SCRIPT_DIR/$script_name" $script_args
        fi
    else
        echo "[ERROR] Script not found: $script_name"
        exit 1
    fi
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            DO_CLEAN=true
            shift
            ;;
        -b|--build)
            DO_BUILD=true
            shift
            ;;
        -f|--flash)
            DO_FLASH=true
            shift
            ;;
        -t|--test)
            DO_TEST=true
            shift
            ;;
        -s|--submodule)
            DO_SUBMODULE=true
            shift
            ;;
        -a|--all)
            DO_SUBMODULE=true
            DO_CLEAN=true
            DO_BUILD=true
            DO_FLASH=true
            DO_TEST=true
            shift
            ;;
        -bf)
            DO_BUILD=true
            DO_FLASH=true
            shift
            ;;
        -bt)
            DO_BUILD=true
            DO_TEST=true
            shift
            ;;
        -cf)
            DO_CLEAN=true
            DO_BUILD=true
            DO_FLASH=true
            shift
            ;;
        -ct)
            DO_CLEAN=true
            DO_BUILD=true
            DO_TEST=true
            shift
            ;;
        -cbt)
            DO_CLEAN=true
            DO_BUILD=true
            DO_TEST=true
            shift
            ;;
        -cbf)
            DO_CLEAN=true
            DO_BUILD=true
            DO_FLASH=true
            shift
            ;;
        --dev)
            # Development mode: submodule, clean, build, test (no flash)
            DO_SUBMODULE=true
            DO_CLEAN=true
            DO_BUILD=true
            DO_TEST=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Check if no options were provided
if [[ "$DO_CLEAN" == false && "$DO_BUILD" == false && "$DO_FLASH" == false && "$DO_TEST" == false && "$DO_SUBMODULE" == false ]]; then
    echo "No tasks specified. Use --help for usage information."
    exit 1
fi

# Show execution plan
echo "[DEV] STM32 BMS Development Workflow"
echo "===================================="
echo "[INFO] Working directory: $BMS_DIR"
echo "[INFO] Script directory: $SCRIPT_DIR"

if [[ "$DRY_RUN" == true ]]; then
    echo "[INFO] DRY RUN MODE - No actual execution"
fi

echo ""
echo "Execution plan:"
[[ "$DO_SUBMODULE" == true ]] && echo "  ✓ Update submodules"
[[ "$DO_CLEAN" == true ]] && echo "  ✓ Clean build directories"
[[ "$DO_BUILD" == true ]] && echo "  ✓ Build STM32 firmware"
[[ "$DO_TEST" == true ]] && echo "  ✓ Run unit tests"
[[ "$DO_FLASH" == true ]] && echo "  ✓ Flash firmware to device"

if [[ "$DRY_RUN" == false ]]; then
    echo ""
    read -p "Continue? [Y/n] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        echo "Aborted by user."
        exit 0
    fi
fi

# Execute tasks in order
START_TIME=$(date +%s)

if [[ "$DO_SUBMODULE" == true ]]; then
    execute_step "SUBMODULE" "submodule.sh --update"
fi

if [[ "$DO_CLEAN" == true ]]; then
    execute_step "CLEAN" "clean.sh"
fi

if [[ "$DO_BUILD" == true ]]; then
    execute_step "BUILD" "build.sh"
fi

if [[ "$DO_TEST" == true ]]; then
    execute_step "TEST" "test.sh"
fi

if [[ "$DO_FLASH" == true ]]; then
    execute_step "FLASH" "flash.sh"
fi

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo ""
echo "============================================"
echo "[SUCCESS] Development workflow completed!"
echo "[INFO] Total time: ${DURATION}s"
echo "[INFO] Finished at: $(date)"
echo "============================================"