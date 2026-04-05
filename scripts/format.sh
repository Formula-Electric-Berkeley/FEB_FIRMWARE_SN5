#!/bin/bash
#
# Clang-Format Script for User Code
#
# Formats all .c and .h files in Core/User/ directories across all boards.
# Requires clang-format version 18 for consistent formatting across all platforms.
# Will offer to install the correct version if not found.
#
# Usage:
#   ./scripts/format.sh           # Format all user code in-place
#   ./scripts/format.sh --check   # Check only (CI mode, exits 1 if changes needed)
#   ./scripts/format.sh -h        # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

# Required clang-format major version (must match CI)
REQUIRED_VERSION=18

# Available boards
BOARDS=("BMS" "DASH" "DART" "DCU" "LVPDB" "PCU" "Sensor_Nodes" "UART_TEST")

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

Formats all .c and .h files in Core/User/ directories across all boards.
Uses the .clang-format configuration file in the repository root.

IMPORTANT: Requires clang-format version ${REQUIRED_VERSION} for consistent formatting.
           Will offer to install automatically if not found.

Usage:
  ./scripts/format.sh           Format all user code in-place
  ./scripts/format.sh --check   Check only (CI mode, exits 1 if changes needed)
  ./scripts/format.sh -h        Show this help

Installation (manual):
  macOS:   brew install llvm@${REQUIRED_VERSION}
  Ubuntu:  sudo apt install clang-format-${REQUIRED_VERSION}
  Windows: winget install LLVM.LLVM --version 18.1.8
           or download from https://github.com/llvm/llvm-project/releases

Boards: ${BOARDS[*]}

Examples:
  ./scripts/format.sh           # Fix formatting for all boards
  ./scripts/format.sh --check   # Verify formatting (use in CI)
EOF
}

detect_os() {
    case "$(uname -s)" in
        Darwin*)          echo "macos" ;;
        Linux*)           echo "linux" ;;
        MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
        *)                echo "unknown" ;;
    esac
}

install_clang_format() {
    local os=$(detect_os)

    echo ""
    log_warn "clang-format ${REQUIRED_VERSION} is required but not found."
    echo ""

    case "$os" in
        macos)
            echo "To install on macOS, run:"
            echo "  brew install llvm@${REQUIRED_VERSION}"
            echo ""
            read -p "Install now? [y/N] " -n 1 -r
            echo ""
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                log_info "Installing llvm@${REQUIRED_VERSION} via Homebrew..."
                if ! command -v brew &> /dev/null; then
                    log_error "Homebrew not found. Please install Homebrew first:"
                    echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                    exit 1
                fi
                brew install llvm@${REQUIRED_VERSION}
                log_info "Installation complete!"
                return 0
            else
                log_error "Installation cancelled."
                exit 1
            fi
            ;;
        linux)
            echo "To install on Ubuntu/Debian, run:"
            echo "  sudo apt update && sudo apt install clang-format-${REQUIRED_VERSION}"
            echo ""
            read -p "Install now? [y/N] " -n 1 -r
            echo ""
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                log_info "Installing clang-format-${REQUIRED_VERSION} via apt..."
                sudo apt update && sudo apt install -y clang-format-${REQUIRED_VERSION}
                log_info "Installation complete!"
                return 0
            else
                log_error "Installation cancelled."
                exit 1
            fi
            ;;
        windows)
            echo "To install on Windows, use one of these methods:"
            echo ""
            echo "  Option 1 - winget (recommended):"
            echo "    winget install LLVM.LLVM --version 18.1.8"
            echo ""
            echo "  Option 2 - Download installer:"
            echo "    https://github.com/llvm/llvm-project/releases/tag/llvmorg-18.1.8"
            echo "    Download: LLVM-18.1.8-win64.exe"
            echo ""
            echo "  After installing, ensure LLVM is in your PATH:"
            echo "    Add C:\\Program Files\\LLVM\\bin to your system PATH"
            echo ""
            read -p "Try installing via winget now? [y/N] " -n 1 -r
            echo ""
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                log_info "Installing LLVM via winget..."
                if command -v winget &> /dev/null; then
                    winget install LLVM.LLVM --version 18.1.8
                    log_info "Installation complete! You may need to restart Git Bash."
                    return 0
                else
                    log_error "winget not found. Please install manually from:"
                    echo "  https://github.com/llvm/llvm-project/releases/tag/llvmorg-18.1.8"
                    exit 1
                fi
            else
                log_error "Installation cancelled."
                exit 1
            fi
            ;;
        *)
            log_error "Unknown OS. Please install clang-format ${REQUIRED_VERSION} manually."
            exit 1
            ;;
    esac
}

find_clang_format() {
    # Try versioned binary first (Linux)
    if command -v clang-format-${REQUIRED_VERSION} &> /dev/null; then
        echo "clang-format-${REQUIRED_VERSION}"
        return
    fi

    # macOS Apple Silicon Homebrew location
    if [[ -x "/opt/homebrew/opt/llvm@${REQUIRED_VERSION}/bin/clang-format" ]]; then
        echo "/opt/homebrew/opt/llvm@${REQUIRED_VERSION}/bin/clang-format"
        return
    fi

    # macOS Intel Homebrew location
    if [[ -x "/usr/local/opt/llvm@${REQUIRED_VERSION}/bin/clang-format" ]]; then
        echo "/usr/local/opt/llvm@${REQUIRED_VERSION}/bin/clang-format"
        return
    fi

    # Windows - check common LLVM install locations
    if [[ -x "/c/Program Files/LLVM/bin/clang-format.exe" ]]; then
        echo "/c/Program Files/LLVM/bin/clang-format.exe"
        return
    fi

    # Windows - try PATH
    if command -v clang-format &> /dev/null; then
        local cf_path=$(command -v clang-format)
        # Return it, will be version-checked later
        echo "$cf_path"
        return
    fi

    echo ""
}

check_clang_format() {
    CLANG_FORMAT=$(find_clang_format)

    # If not found, offer to install
    if [[ -z "$CLANG_FORMAT" ]]; then
        install_clang_format
        # Re-check after installation
        CLANG_FORMAT=$(find_clang_format)
        if [[ -z "$CLANG_FORMAT" ]]; then
            log_error "clang-format ${REQUIRED_VERSION} still not found after installation."
            log_error "You may need to restart your terminal or check your PATH."
            exit 1
        fi
    fi

    # Verify version
    VERSION_OUTPUT=$("$CLANG_FORMAT" --version)
    VERSION=$(echo "$VERSION_OUTPUT" | grep -oE '[0-9]+\.[0-9]+' | head -1)
    MAJOR=$(echo "$VERSION" | cut -d. -f1)

    if [[ "$MAJOR" != "$REQUIRED_VERSION" ]]; then
        log_error "clang-format version mismatch!"
        echo ""
        echo "  Found:    v${VERSION} ($CLANG_FORMAT)"
        echo "  Required: v${REQUIRED_VERSION}.x"
        echo ""
        install_clang_format
        # Re-check after installation
        CLANG_FORMAT=$(find_clang_format)
        if [[ -z "$CLANG_FORMAT" ]]; then
            log_error "Installation failed. Please install manually."
            exit 1
        fi
        VERSION_OUTPUT=$("$CLANG_FORMAT" --version)
        VERSION=$(echo "$VERSION_OUTPUT" | grep -oE '[0-9]+\.[0-9]+' | head -1)
    fi

    log_info "Using $CLANG_FORMAT (v${VERSION})"
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

# Also format common libraries (Inc/ and Src/ directories)
for lib in "${COMMON_LIBS[@]}"; do
    if [[ -d "$lib" ]]; then
        while IFS= read -r -d '' file; do
            FILES+=("$file")
        done < <(find "$lib" -type f \( -name "*.c" -o -name "*.h" \) -print0 2>/dev/null)
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
