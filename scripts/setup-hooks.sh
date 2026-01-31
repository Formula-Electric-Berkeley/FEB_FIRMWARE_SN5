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

# Install pre-commit via pip
install_precommit() {
    log_info "Installing pre-commit..."

    if command -v pip3 &> /dev/null; then
        pip3 install pre-commit
    elif command -v pip &> /dev/null; then
        pip install pre-commit
    else
        log_error "pip not found. Cannot install pre-commit."
        echo ""
        echo "Install pip first, or install pre-commit manually:"
        echo "  - macOS:   brew install pre-commit"
        echo "  - Ubuntu:  sudo apt install pre-commit"
        echo "  - pip:     pip install pre-commit"
        return 1
    fi
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
