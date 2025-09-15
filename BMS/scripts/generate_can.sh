#!/bin/bash

# CAN Library Generation Script for STM32 BMS Project
# Usage: ./generate_can.sh [OPTIONS]

set -e  # Exit on any error

# Script is in scripts directory, need to work from BMS directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BMS_DIR="$(dirname "$SCRIPT_DIR")"
CAN_DIR="$BMS_DIR/FEB_CAN_Library_SN4"

# Initialize flags
DRY_RUN=false
QUIET=false

# Function to show help
show_help() {
    echo "CAN Library Generation Script for STM32 BMS Project"
    echo ""
    echo "Usage: ./generate_can.sh [OPTIONS]"
    echo ""
    echo "DESCRIPTION:"
    echo "  Generates C source files from Python CAN message definitions using cantools"
    echo ""
    echo "PROCESS:"
    echo "  1. Run generate.py to create DBC file from Python message definitions"
    echo "  2. Use cantools to generate C source files from DBC file"
    echo ""
    echo "OPTIONS:"
    echo "  --dry-run        Show what would be executed without running"
    echo "  -q, --quiet      Reduce output verbosity"
    echo "  -h, --help       Show this help message"
    echo ""
    echo "EXAMPLES:"
    echo "  ./generate_can.sh           Generate CAN library files"
    echo "  ./generate_can.sh --dry-run Preview generation commands"
    echo "  ./generate_can.sh -q        Generate with minimal output"
    echo ""
    echo "REQUIREMENTS:"
    echo "  - Python 3 with cantools package installed"
    echo "  - FEB_CAN_Library_SN4 submodule must be present"
    echo "  - Optional: Conda/Miniconda with 'feb_can' environment"
    echo ""
    echo "NOTES:"
    echo "  - Generated files: gen/FEB_CAN.dbc, gen/feb_can.c, gen/feb_can.h"
    echo "  - Run from the scripts directory"
    echo "  - This script is automatically called during submodule updates"
    exit 0
}

# Function to execute or show command
execute_command() {
    local cmd="$1"
    local description="$2"
    local working_dir="$3"
    
    if [[ "$QUIET" == false ]]; then
        echo "[CAN-GEN] $description"
    fi
    
    if [[ "$DRY_RUN" == true ]]; then
        echo "[DRY RUN] Would execute in $working_dir: $cmd"
        return 0
    fi
    
    if [[ "$QUIET" == false ]]; then
        echo "[EXEC] $cmd"
    fi
    
    # Execute command in the specified directory
    (cd "$working_dir" && eval "$cmd")
}

# Function to check requirements
check_requirements() {
    # Check if conda is available and try to use it
    if command -v conda &> /dev/null; then
        # Try to activate conda environment if it exists
        if conda env list | grep -q "feb_can"; then
            if [[ "$QUIET" == false ]]; then
                echo "[INFO] Activating conda environment 'feb_can'"
            fi
            source "$(conda info --base)/etc/profile.d/conda.sh"
            conda activate feb_can
        else
            if [[ "$QUIET" == false ]]; then
                echo "[INFO] Conda found but 'feb_can' environment not found"
                echo "[INFO] Recommend: conda create -n feb_can python=3.8 && conda activate feb_can && pip install cantools"
            fi
        fi
    else
        if [[ "$QUIET" == false ]]; then
            echo "[INFO] Conda not found, using system Python (this is fine)"
        fi
    fi
    
    # Check if Python 3 is available
    if ! command -v python3 &> /dev/null && ! command -v python &> /dev/null; then
        echo "[ERROR] Python is not installed or not in PATH"
        exit 1
    fi
    
    # Use python3 if available, otherwise python
    PYTHON_CMD="python3"
    if ! command -v python3 &> /dev/null; then
        PYTHON_CMD="python"
    fi
    
    # Check if cantools is installed
    if ! $PYTHON_CMD -c "import cantools" 2>/dev/null; then
        echo "[ERROR] cantools package is not installed in current environment"
        echo "[INFO] Install with: pip install cantools"
        echo "[INFO] Or if using conda: conda install -c conda-forge cantools"
        exit 1
    fi
    
    # Check if CAN library directory exists
    if [[ ! -d "$CAN_DIR" ]]; then
        echo "[ERROR] FEB_CAN_Library_SN4 directory not found"
        echo "[INFO] Expected at: $CAN_DIR"
        echo "[INFO] Make sure the submodule is initialized"
        exit 1
    fi
    
    # Check if generate.py exists
    if [[ ! -f "$CAN_DIR/generate.py" ]]; then
        echo "[ERROR] generate.py not found in CAN library"
        exit 1
    fi
    
    # Create gen directory if it doesn't exist
    if [[ ! -d "$CAN_DIR/gen" ]]; then
        if [[ "$DRY_RUN" == false ]]; then
            mkdir -p "$CAN_DIR/gen"
        fi
        if [[ "$QUIET" == false ]]; then
            echo "[INFO] Created gen directory"
        fi
    fi
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -q|--quiet)
            QUIET=true
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

# Change to BMS directory
cd "$BMS_DIR"

echo "[CAN-GEN] STM32 BMS CAN Library Generation"
echo "=========================================="
echo "[INFO] Working directory: $BMS_DIR"
echo "[INFO] CAN library directory: $CAN_DIR"

if [[ "$DRY_RUN" == true ]]; then
    echo "[INFO] DRY RUN MODE - No actual execution"
fi

echo ""

# Check requirements
if [[ "$QUIET" == false ]]; then
    echo "[CHECK] Verifying requirements..."
fi
check_requirements

# Step 1: Generate DBC file from Python message definitions
execute_command "$PYTHON_CMD generate.py" "Generating DBC file from Python message definitions" "$CAN_DIR"

# Step 2: Generate C source files from DBC file
execute_command "$PYTHON_CMD -m cantools generate_c_source -o gen gen/FEB_CAN.dbc" "Generating C source files from DBC file" "$CAN_DIR"

# Verify generated files exist
if [[ "$DRY_RUN" == false ]]; then
    if [[ -f "$CAN_DIR/gen/FEB_CAN.dbc" && -f "$CAN_DIR/gen/feb_can.c" && -f "$CAN_DIR/gen/feb_can.h" ]]; then
        if [[ "$QUIET" == false ]]; then
            echo ""
            echo "[SUCCESS] Generated files:"
            echo "  - $CAN_DIR/gen/FEB_CAN.dbc"
            echo "  - $CAN_DIR/gen/feb_can.c"
            echo "  - $CAN_DIR/gen/feb_can.h"
            
            # Show file sizes
            echo ""
            echo "[INFO] File sizes:"
            ls -lh "$CAN_DIR/gen/FEB_CAN.dbc" "$CAN_DIR/gen/feb_can.c" "$CAN_DIR/gen/feb_can.h" | awk '{print "  - " $9 ": " $5}'
        fi
    else
        echo "[ERROR] Some generated files are missing"
        exit 1
    fi
fi

echo ""
echo "============================================"
echo "[SUCCESS] CAN library generation completed!"
echo "[INFO] Finished at: $(date)"
echo "============================================"