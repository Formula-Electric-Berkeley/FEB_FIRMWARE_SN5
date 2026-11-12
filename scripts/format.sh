#!/bin/bash
#
# Clang-Format Script for User Code
#
# Formats all .c/.h/.cpp/.hpp files in Core/User/ directories across all boards.
# Uses clang-format 19.1.2
#
# Usage:
#   ./scripts/format.sh           # Format all user code in-place
#   ./scripts/format.sh --check   # Check only (CI mode, exits 1 if changes needed)
#   ./scripts/format.sh -h        # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

CLANG_FORMAT_VERSION=19.1.2
CLANG_FORMAT_VENV="$REPO_ROOT/build/.tools/clang-format-$CLANG_FORMAT_VERSION"

# Available boards
source "$SCRIPT_DIR/lib/boards.sh"
BOARDS=("${FEB_BOARD_DIRS[@]}")

# Common libraries to format
COMMON_LIBS=("common/FEB_Serial_Library")

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

Formats all .c/.h/.cpp/.hpp files in Core/User/ directories across all boards.
Uses the .clang-format configuration file in the repository root.


Boards: ${BOARDS[*]}

Examples:
  ./scripts/format.sh           # Fix formatting for all boards
  ./scripts/format.sh --check   # Verify formatting (use in CI)
EOF
}

clang_format_version_of() {
    "$1" --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1
}

find_clang_format() {
    local candidate
    for candidate in "$(command -v clang-format 2>/dev/null || true)" \
                     "$CLANG_FORMAT_VENV/bin/clang-format" \
                     "$CLANG_FORMAT_VENV/Scripts/clang-format.exe"; do
        if [[ -n "$candidate" && -x "$candidate" \
              && "$(clang_format_version_of "$candidate")" == "$CLANG_FORMAT_VERSION" ]]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

install_clang_format() {
    local python
    python="$(command -v python3 || command -v python || true)"
    if [[ -z "$python" ]]; then
        log_error "python3 is required to install clang-format ${CLANG_FORMAT_VERSION}."
        exit 1
    fi

    log_info "Installing clang-format ${CLANG_FORMAT_VERSION} into build/.tools/ ..."
    rm -rf "$CLANG_FORMAT_VENV"
    "$python" -m venv "$CLANG_FORMAT_VENV"
    local pip="$CLANG_FORMAT_VENV/bin/pip"
    [[ -x "$pip" ]] || pip="$CLANG_FORMAT_VENV/Scripts/pip.exe"
    "$pip" install --quiet --disable-pip-version-check "clang-format==${CLANG_FORMAT_VERSION}"
}

check_clang_format() {
    if ! CLANG_FORMAT=$(find_clang_format); then
        install_clang_format
        if ! CLANG_FORMAT=$(find_clang_format); then
            log_error "clang-format ${CLANG_FORMAT_VERSION} could not be installed."
            exit 1
        fi
    fi
    log_info "Using $CLANG_FORMAT (v${CLANG_FORMAT_VERSION})"
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
        done < <(find "$user_dir" -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" \) -print0 2>/dev/null)
    fi
done

# Also format common libraries (Inc/ and Src/ directories)
for lib in "${COMMON_LIBS[@]}"; do
    if [[ -d "$lib" ]]; then
        while IFS= read -r -d '' file; do
            FILES+=("$file")
        done < <(find "$lib" -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" \) -print0 2>/dev/null)
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
        if ! "$CLANG_FORMAT" --dry-run --Werror "$file" 2>/dev/null; then
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
        "$CLANG_FORMAT" -i "$file"
        echo "  Formatted: $file"
    done

    echo ""
    log_info "Done! Formatted ${#FILES[@]} files."
fi
