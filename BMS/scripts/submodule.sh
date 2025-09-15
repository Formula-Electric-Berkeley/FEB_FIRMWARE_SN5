#!/bin/bash

# Submodule Management Script for STM32 BMS Project
# Usage: ./submodule.sh [COMMAND] [OPTIONS]

set -e  # Exit on any error

# Script is in scripts directory, need to work from BMS directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BMS_DIR="$(dirname "$SCRIPT_DIR")"

# Function to show help
show_help() {
    echo "Submodule Management Script for STM32 BMS Project"
    echo ""
    echo "Usage: ./submodule.sh [OPTIONS]"
    echo ""
    echo "COMMANDS:"
    echo "  -i, --init       Initialize all submodules"
    echo "  -u, --update     Update all submodules to latest commit"
    echo "  -p, --pull       Update submodules from remote (same as --update)"
    echo "  -y, --sync       Sync submodule URLs with .gitmodules"
    echo "  -s, --status     Show status of all submodules"
    echo "  -r, --reset      Reset submodules to commit specified in main repo"
    echo ""
    echo "OPTIONS:"
    echo "  --dry-run        Show what would be executed without running"
    echo "  -h, --help       Show this help message"
    echo ""
    echo "EXAMPLES:"
    echo "  ./submodule.sh -i            Initialize submodules after cloning"
    echo "  ./submodule.sh --update      Update to latest CAN library"
    echo "  ./submodule.sh -s            Check submodule status"
    echo "  ./submodule.sh --dry-run -u  See what update would do"
    echo ""
    echo "NOTES:"
    echo "  - Run from the scripts directory"
    echo "  - Updates will pull latest changes from FEB_CAN_Library_SN4"
    echo "  - After updates, commit changes to main repo to lock version"
    exit 0
}

# Initialize flags
DRY_RUN=false
COMMAND=""

# Function to execute or show command
execute_command() {
    local cmd="$1"
    local description="$2"
    
    echo "[SUBMODULE] $description"
    
    if [[ "$DRY_RUN" == true ]]; then
        echo "[DRY RUN] Would execute: $cmd"
        return 0
    fi
    
    echo "[EXEC] $cmd"
    eval "$cmd"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -i|--init)
            COMMAND="init"
            shift
            ;;
        -u|--update|-p|--pull)
            COMMAND="update"
            shift
            ;;
        -y|--sync)
            COMMAND="sync"
            shift
            ;;
        -s|--status)
            COMMAND="status"
            shift
            ;;
        -r|--reset)
            COMMAND="reset"
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
            echo "Unknown command/option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Check if command was provided
if [[ -z "$COMMAND" ]]; then
    echo "No command specified. Use --help for usage information."
    exit 1
fi

# Change to BMS directory
cd "$BMS_DIR"

echo "[SUBMODULE] STM32 BMS Submodule Management"
echo "========================================"
echo "[INFO] Working directory: $BMS_DIR"
echo "[INFO] Command: $COMMAND"

if [[ "$DRY_RUN" == true ]]; then
    echo "[INFO] DRY RUN MODE - No actual execution"
fi

echo ""

# Execute commands
case "$COMMAND" in
    init)
        execute_command "git submodule init" "Initializing submodules"
        execute_command "git submodule update" "Updating submodules to specified commits"
        ;;
    update)
        execute_command "git submodule update --remote --merge" "Updating submodules to latest remote commits"
        
        # Generate CAN library files after update
        echo ""
        echo "[INFO] Generating CAN library C/H files..."
        if [[ -f "$SCRIPT_DIR/generate_can.sh" ]]; then
            if [[ "$DRY_RUN" == true ]]; then
                echo "[DRY RUN] Would execute: generate_can.sh --quiet"
            else
                "$SCRIPT_DIR/generate_can.sh" --quiet
                if [[ $? -ne 0 ]]; then
                    echo "[WARNING] CAN library generation failed, but submodule update completed"
                else
                    echo "[SUCCESS] CAN library files generated successfully"
                fi
            fi
        else
            echo "[WARNING] generate_can.sh not found, skipping CAN library generation"
        fi
        
        echo ""
        echo "[INFO] Submodules updated to latest versions."
        echo "[INFO] Remember to commit these changes to lock the new versions:"
        echo "       git add .gitmodules FEB_CAN_Library_SN4"
        echo "       git commit -m 'Update FEB CAN Library submodule'"
        ;;
    sync)
        execute_command "git submodule sync" "Synchronizing submodule URLs"
        execute_command "git submodule update --init --recursive" "Updating after sync"
        ;;
    status)
        echo "[INFO] Submodule status:"
        execute_command "git submodule status" "Checking submodule status"
        echo ""
        echo "[INFO] Submodule summary:"
        if [[ "$DRY_RUN" == false ]]; then
            git submodule foreach 'echo "  $(basename $PWD): $(git rev-parse --short HEAD) ($(git describe --always --dirty))"'
        else
            echo "[DRY RUN] Would show submodule summary"
        fi
        ;;
    reset)
        execute_command "git submodule update --init" "Resetting submodules to committed versions"
        ;;
esac

echo ""
echo "============================================"
echo "[SUCCESS] Submodule operation completed!"
echo "[INFO] Finished at: $(date)"
echo "============================================"