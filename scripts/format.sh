#!/bin/bash
#
# Clang-Format Script for User Code
#
# Formats all .c and .h files in Core/User/ directories across all boards.
#
# Usage:
#   ./scripts/format.sh           # Format all user code in-place
#   ./scripts/format.sh --check   # Check only (CI mode, exits 1 if changes needed)
#   ./scripts/format.sh -h        # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

# Available boards
BOARDS=("BMS" "DASH" "DART" "DCU" "LVPDB" "PCU" "Sensor_Nodes")

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

show_help() {
    cat << EOF
Clang-Format Script for User Code

Formats all .c and .h files in Core/User/ directories across all boards.
Uses the .clang-format configuration file in the repository root.

Usage:
  ./scripts/format.sh           Format all user code in-place
  ./scripts/format.sh --check   Check only (CI mode, exits 1 if changes needed)
  ./scripts/format.sh -h        Show this help

Boards: ${BOARDS[*]}

Examples:
  ./scripts/format.sh           # Fix formatting for all boards
  ./scripts/format.sh --check   # Verify formatting (use in CI)
EOF
}

check_clang_format() {
    if ! command -v clang-format &> /dev/null; then
        log_error "clang-format not found in PATH"
        echo ""
        echo "Install clang-format:"
        echo "  macOS:   brew install clang-format"
        echo "  Ubuntu:  sudo apt install clang-format"
        echo "  Windows: Install LLVM from https://releases.llvm.org/"
        exit 1
    fi
}

# Main script
CHECK_MODE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)
            CHECK_MODE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

check_clang_format

cd "$REPO_ROOT"

# Find all .c and .h files in Core/User/ directories
FILES=()
for board in "${BOARDS[@]}"; do
    user_dir="$board/Core/User"
    if [[ -d "$user_dir" ]]; then
        while IFS= read -r -d '' file; do
            FILES+=("$file")
        done < <(find "$user_dir" -type f \( -name "*.c" -o -name "*.h" \) -print0 2>/dev/null)
    fi
done

if [[ ${#FILES[@]} -eq 0 ]]; then
    log_warn "No .c or .h files found in Core/User/ directories"
    exit 0
fi

log_header "Clang-Format"

if [[ "$CHECK_MODE" == true ]]; then
    log_info "Checking formatting (${#FILES[@]} files)..."

    FAILED=false
    for file in "${FILES[@]}"; do
        if ! clang-format --dry-run --Werror "$file" 2>/dev/null; then
            log_error "Formatting issue: $file"
            FAILED=true
        fi
    done

    if [[ "$FAILED" == true ]]; then
        echo ""
        log_error "Formatting check failed. Run './scripts/format.sh' to fix."
        exit 1
    else
        log_info "All files formatted correctly"
        exit 0
    fi
else
    log_info "Formatting ${#FILES[@]} files..."

    for file in "${FILES[@]}"; do
        clang-format -i "$file"
        echo "  Formatted: $file"
    done

    echo ""
    log_info "Done! Formatted ${#FILES[@]} files."
fi
