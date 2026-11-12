#!/bin/bash
#
# CAN Library Generation Script
#
# Usage:
#   ./generate_can.sh                  # Generate CAN library files
#   ./generate_can.sh --check          # Check if files are up to date (for CI)
#   ./generate_can.sh --list   | -l    # List all registered messages
#   ./generate_can.sh --ids    | -i    # Show frame ID allocation map
#   ./generate_can.sh --validate | -v  # Validate message definitions
#   ./generate_can.sh --dump   | -d    # Visualize the DBC file
#   ./generate_can.sh --help   | -h    # Show this help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"
CANTOOLS_VERSION="39.4.13"
GEN_DIR="$SCRIPT_DIR/gen"
GEN_FILES="FEB_CAN.dbc feb_can_db.h feb_can_db.c feb_can_traits.hpp"

SCRATCH_ROOT=""
SCRATCH_CANTOOLS=""
SCRATCH_COMPARE=""

scratch_init() {
    [ -n "$SCRATCH_ROOT" ] && return 0

    SCRATCH_ROOT=$(mktemp -d) || { echo "[ERROR] mktemp -d failed" >&2; exit 1; }
    if [ -z "$SCRATCH_ROOT" ] || [ ! -d "$SCRATCH_ROOT" ]; then
        echo "[ERROR] mktemp -d returned no usable directory" >&2
        exit 1
    fi

    SCRATCH_CANTOOLS="$SCRATCH_ROOT/cantools"
    SCRATCH_COMPARE="$SCRATCH_ROOT/compare"
    mkdir -p "$SCRATCH_CANTOOLS" "$SCRATCH_COMPARE"
}

cleanup_scratch() {
    [ -n "$SCRATCH_ROOT" ] && [ -d "$SCRATCH_ROOT" ] || return 0

    rm -f "$SCRATCH_CANTOOLS/feb_can.h" "$SCRATCH_CANTOOLS/feb_can.c"
    for _f in $GEN_FILES; do
        rm -f "$SCRATCH_COMPARE/$_f.orig" "$SCRATCH_COMPARE/$_f.new"
    done

    rmdir "$SCRATCH_CANTOOLS" "$SCRATCH_COMPARE" "$SCRATCH_ROOT" 2>/dev/null \
        || echo "[WARN] scratch dir not empty, left for inspection: $SCRATCH_ROOT" >&2
}

trap cleanup_scratch EXIT

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}\n"
}

# Set up virtual environment and install cantools
setup_env() {
    if [ ! -d "$VENV_DIR" ]; then
        log_info "Creating virtual environment..."
        python3 -m venv "$VENV_DIR"
    fi

    # Activate venv
    source "$VENV_DIR/bin/activate"

    # Check if cantools is installed with correct version
    INSTALLED_VERSION=$(python -c "import cantools; print(cantools.__version__)" 2>/dev/null || echo "none")
    if [ "$INSTALLED_VERSION" != "$CANTOOLS_VERSION" ]; then
        log_info "Installing cantools==$CANTOOLS_VERSION..."
        pip install --quiet cantools==$CANTOOLS_VERSION
    fi
}


# Generate CAN library files
generate() {
    cd "$SCRIPT_DIR"

    scratch_init
    CT_DIR="$SCRATCH_CANTOOLS"

    log_info "Generating DBC file from Python definitions..."
    python generate.py

    log_info "Generating C source from DBC (cantools -> scratch)..."
    python -m cantools generate_c_source -o "$CT_DIR" gen/FEB_CAN.dbc

    log_info "Consolidating into gen/feb_can_db.{h,c}..."
    python generate.py --emit-db "$CT_DIR"

    log_info "Generation complete!"
}

# Check mode for CI - compares generated files without modifying committed files
check() {
    cd "$SCRIPT_DIR"

    scratch_init
    TEMP_DIR="$SCRATCH_COMPARE"
    CT_DIR="$SCRATCH_CANTOOLS"

    for f in $GEN_FILES; do
        cp "$GEN_DIR/$f" "$TEMP_DIR/$f.orig"
    done

    log_info "Generating fresh files for comparison..."
    log_info "Using cantools version: $(python -c 'import cantools; print(cantools.__version__)')"
    log_info "Python version: $(python --version)"

    python generate.py
    python -m cantools generate_c_source -o "$CT_DIR" gen/FEB_CAN.dbc
    python generate.py --emit-db "$CT_DIR"

    for f in $GEN_FILES; do
        cp "$GEN_DIR/$f" "$TEMP_DIR/$f.new"
        cp "$TEMP_DIR/$f.orig" "$GEN_DIR/$f"
    done

    DIFF_FOUND=false

    log_info "Comparing generated files..."
    for f in $GEN_FILES; do
        if ! diff -q "$TEMP_DIR/$f.new" "$TEMP_DIR/$f.orig" > /dev/null 2>&1; then
            log_error "gen/$f differs from committed version"
            echo "--- Diff output ---"
            diff "$TEMP_DIR/$f.orig" "$TEMP_DIR/$f.new" | head -50 || true
            echo "-------------------"
            DIFF_FOUND=true
        else
            echo "  gen/$f: OK"
        fi
    done

    if [ "$DIFF_FOUND" = true ]; then
        echo ""
        log_error "CAN library files are out of date!"
        echo ""
        echo "To fix, run:"
        echo "  cd common/FEB_CAN_Definitions"
        echo "  ./generate_can.sh"
        echo ""
        exit 1
    else
        echo ""
        log_info "All generated files are up to date."
    fi
}


# List all registered messages
cmd_list() {
    cd "$SCRIPT_DIR"
    log_header "Registered CAN Messages"
    python generate.py --list
}

# Show frame ID allocation
cmd_ids() {
    cd "$SCRIPT_DIR"
    log_header "Frame ID Allocation"
    python generate.py --ids
}

# Validate message definitions
cmd_validate() {
    cd "$SCRIPT_DIR"
    log_header "Validating Message Registry"
    if python generate.py --validate; then
        log_info "All validations passed!"
    else
        log_error "Validation failed!"
        exit 1
    fi
}

# Dump/visualize DBC file
cmd_dump() {
    cd "$SCRIPT_DIR"
    log_header "DBC File Contents"
    if [ -f "$GEN_DIR/FEB_CAN.dbc" ]; then
        python -m cantools dump "$GEN_DIR/FEB_CAN.dbc"
    else
        log_error "DBC file not found. Run './generate_can.sh' first."
        exit 1
    fi
}

# Show help
cmd_help() {
    echo "FEB CAN Library Generator"
    echo ""
    echo "Usage: ./generate_can.sh [option]"
    echo ""
    echo "Options:"
    echo "  (none)              Generate CAN library files (default)"
    echo "  -c, --check         Check if files are up to date (for CI)"
    echo "  -l, --list          List all registered messages with IDs"
    echo "  -i, --ids           Show frame ID allocation map"
    echo "  -v, --validate      Validate message definitions only"
    echo "  -d, --dump          Visualize the generated DBC file"
    echo "  -h, --help          Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./generate_can.sh              # Regenerate all files"
    echo "  ./generate_can.sh --list       # See what messages exist"
    echo "  ./generate_can.sh --ids        # Check available ID ranges"
    echo "  ./generate_can.sh --check      # CI mode: verify files are current"
}

# Main
main() {
    setup_env

    case "${1:-}" in
        -c|--check)
            check
            ;;
        -l|--list)
            cmd_list
            ;;
        -i|--ids)
            cmd_ids
            ;;
        -v|--validate)
            cmd_validate
            ;;
        -d|--dump)
            cmd_dump
            ;;
        -h|--help)
            cmd_help
            ;;
        "")
            generate
            ;;
        *)
            log_error "Unknown option: $1"
            echo ""
            cmd_help
            exit 1
            ;;
    esac
}

main "$@"
