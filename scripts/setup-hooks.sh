#!/bin/bash
#
# Pre-commit Hooks Setup Script
#
# Installs pre-commit and configures git hooks for the repository.
#
# Usage:
#   ./scripts/setup-hooks.sh           # Install hooks
#   ./scripts/setup-hooks.sh --remove  # Remove hooks
#   ./scripts/setup-hooks.sh -h        # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

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

# Check if pre-commit is installed
check_precommit() {
    if command -v pre-commit &> /dev/null; then
        return 0
    fi
    return 1
}

# Install pre-commit using the best available method
install_precommit() {
    log_info "Installing pre-commit..."

    # macOS: prefer Homebrew (avoids PEP 668 issues)
    if [[ "$(uname -s)" == "Darwin" ]] && command -v brew &> /dev/null; then
        log_info "Using Homebrew to install pre-commit..."
        if brew install pre-commit; then
            return 0
        fi
        log_warn "Homebrew install failed, trying alternatives..."
    fi

    # Try pipx (manages its own virtualenv, works everywhere)
    if command -v pipx &> /dev/null; then
        log_info "Using pipx to install pre-commit..."
        if pipx install pre-commit; then
            return 0
        fi
        log_warn "pipx install failed, trying pip..."
    fi

    # Try pip with --user flag (safer than system-wide)
    if command -v pip3 &> /dev/null; then
        log_info "Using pip3 --user to install pre-commit..."
        if pip3 install --user pre-commit; then
            # Add user bin to PATH hint
            log_warn "You may need to add ~/.local/bin to your PATH"
            return 0
        fi
    elif command -v pip &> /dev/null; then
        log_info "Using pip --user to install pre-commit..."
        if pip install --user pre-commit; then
            log_warn "You may need to add ~/.local/bin to your PATH"
            return 0
        fi
    fi

    # All methods failed
    log_error "Could not install pre-commit automatically."
    echo ""
    echo "Please install pre-commit manually using one of these methods:"
    echo ""
    echo "  macOS (recommended):"
    echo "    brew install pre-commit"
    echo ""
    echo "  Using pipx (any platform):"
    echo "    brew install pipx  # or: sudo apt install pipx"
    echo "    pipx install pre-commit"
    echo ""
    echo "  Linux:"
    echo "    sudo apt install pre-commit"
    echo ""
    echo "Then run this script again."
    return 1
}

# Install git hooks
install_hooks() {
    log_header "Installing Pre-commit Hooks"

    cd "$REPO_ROOT"

    # Check if config exists
    if [ ! -f ".pre-commit-config.yaml" ]; then
        log_error ".pre-commit-config.yaml not found in repository root"
        return 1
    fi

    # Install pre-commit if needed
    if ! check_precommit; then
        install_precommit || return 1
    fi

    # Verify installation
    if ! check_precommit; then
        log_error "pre-commit installation failed"
        return 1
    fi

    log_info "pre-commit version: $(pre-commit --version)"

    # Install hooks
    log_info "Installing git hooks..."
    pre-commit install

    # Also install commit-msg hook for conventional commits (optional)
    # pre-commit install --hook-type commit-msg

    log_info "Hooks installed successfully!"
    echo ""
    echo "Pre-commit will now run automatically on git commit."
    echo ""
    echo "Useful commands:"
    echo "  pre-commit run --all-files    # Run all hooks on all files"
    echo "  pre-commit run clang-format   # Run specific hook"
    echo "  git commit --no-verify        # Skip hooks (use sparingly)"
}

# Remove git hooks
remove_hooks() {
    log_header "Removing Pre-commit Hooks"

    cd "$REPO_ROOT"

    if check_precommit; then
        pre-commit uninstall
        log_info "Hooks removed"
    else
        # Manual removal
        if [ -f ".git/hooks/pre-commit" ]; then
            rm -f ".git/hooks/pre-commit"
            log_info "Hooks removed manually"
        else
            log_info "No hooks to remove"
        fi
    fi
}

# Show help
show_help() {
    cat << EOF
Pre-commit Hooks Setup Script

Installs pre-commit and configures git hooks for code quality.

Usage:
  ./scripts/setup-hooks.sh           Install hooks
  ./scripts/setup-hooks.sh --remove  Remove hooks
  ./scripts/setup-hooks.sh -h        Show this help

Hooks configured:
  - trailing-whitespace    Remove trailing whitespace
  - end-of-file-fixer      Ensure files end with newline
  - check-added-large-files Prevent large files (>500KB)
  - check-merge-conflict   Detect merge conflict markers
  - mixed-line-ending      Enforce LF line endings
  - clang-format           Format C code (Core/User/)
  - cppcheck               Static analysis
  - can-validate           Verify CAN generated files

Requirements:
  - Python 3 with pip (for pre-commit installation)
  - clang-format (for formatting)
  - cppcheck (for static analysis)
EOF
}

# Main
main() {
    case "${1:-}" in
        --remove|-r)
            remove_hooks
            ;;
        -h|--help)
            show_help
            ;;
        "")
            install_hooks
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
}

main "$@"
