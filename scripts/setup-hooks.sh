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

# Interpreter for the pip / `-m pre_commit` fallbacks. An array because the
# Windows launcher form is two words (`py -3`). Set by find_python.
PYTHON=()

# How to invoke pre-commit. Set by resolve_precommit.
PRE_COMMIT=()

# Find a Python 3 that actually runs. `command -v` alone is not enough on
# Windows: the Microsoft Store alias (python3.exe / python.exe under WindowsApps)
# is on PATH but exits non-zero instead of running anything.
find_python() {
    [ "${#PYTHON[@]}" -gt 0 ] && return 0

    local cand
    local -a cmd
    for cand in python3 python "py -3"; do
        # shellcheck disable=SC2206  # intentional word-split of "py -3"
        cmd=($cand)
        if command -v "${cmd[0]}" &> /dev/null &&
            "${cmd[@]}" -c 'import sys; sys.exit(0 if sys.version_info[0] == 3 else 1)' &> /dev/null; then
            PYTHON=("${cmd[@]}")
            return 0
        fi
    done
    return 1
}

# Work out how to invoke pre-commit and store it in PRE_COMMIT.
# Prefers the executable on PATH; falls back to `python -m pre_commit`, which is
# all a `pip install --user` leaves you with when the user scripts dir isn't on
# PATH (the default on Windows). The git hook that `pre-commit install` writes
# embeds the interpreter path and runs `-mpre_commit` itself, so commits work
# either way.
resolve_precommit() {
    if command -v pre-commit &> /dev/null; then
        PRE_COMMIT=(pre-commit)
        return 0
    fi
    if find_python && "${PYTHON[@]}" -m pre_commit --version &> /dev/null; then
        PRE_COMMIT=("${PYTHON[@]}" -m pre_commit)
        return 0
    fi
    return 1
}

# Print the directory `pip install --user` drops executables into, in a form
# usable in this shell's PATH. Ask Python rather than guess: it is versioned on
# Windows (%APPDATA%\Python\Python313\Scripts), ~/.local/bin on Linux, and
# ~/Library/Python/3.x/bin for macOS framework builds.
user_scripts_dir() {
    find_python || return 1

    local dir
    dir="$("${PYTHON[@]}" -c '
import os, sys, sysconfig
try:
    scheme = sysconfig.get_preferred_scheme("user")
except AttributeError:  # Python < 3.10
    if sys.platform == "darwin" and getattr(sys, "_framework", None):
        scheme = "osx_framework_user"
    else:
        scheme = os.name + "_user"
print(sysconfig.get_path("scripts", scheme))
' 2> /dev/null)" || return 1

    dir="${dir%$'\r'}" # Windows Python ends lines with CRLF
    [ -n "$dir" ] || return 1

    # A native C:\... path can't go in a bash PATH (the drive colon is the
    # separator), so convert to /c/... on Git Bash.
    if command -v cygpath &> /dev/null; then
        dir="$(cygpath -u "$dir")"
    fi
    echo "$dir"
}

# After a pip --user install: make the new executable reachable for the rest of
# this run, and print the exact line that makes it permanent.
add_user_scripts_to_path() {
    local dir
    dir="$(user_scripts_dir)" || return 0

    case ":$PATH:" in
        *":$dir:"*) return 0 ;;
    esac

    export PATH="$dir:$PATH"
    log_warn "$dir is not on your PATH."
    echo "  Git hooks will still run (they call Python directly). To use the"
    echo "  'pre-commit' command yourself, add this to your shell profile:"
    echo ""
    echo "    export PATH=\"$dir:\$PATH\""
    echo ""
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

    # Try pip with --user flag (safer than system-wide). Go through the
    # interpreter find_python validated rather than a bare pip3/pip, so the
    # install location and resolve_precommit's `-m pre_commit` fallback agree.
    if find_python && "${PYTHON[@]}" -m pip --version &> /dev/null; then
        log_info "Using '${PYTHON[*]} -m pip install --user' to install pre-commit..."
        if "${PYTHON[@]}" -m pip install --user pre-commit; then
            add_user_scripts_to_path
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
    echo "  Windows (Git Bash, needs Python from python.org):"
    echo "    py -3 -m pip install --user pre-commit"
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
    if ! resolve_precommit; then
        install_precommit || return 1

        # Verify installation. The installer succeeded, so this is a
        # reachability problem, not an install failure — say which.
        if ! resolve_precommit; then
            log_error "pre-commit was installed, but can't be run from this shell."
            echo "  Neither 'pre-commit' nor 'python -m pre_commit' works here."
            local dir
            if dir="$(user_scripts_dir)"; then
                echo "  Add this directory to your PATH, then run this script again:"
                echo "    $dir"
            fi
            return 1
        fi
    fi

    log_info "pre-commit version: $("${PRE_COMMIT[@]}" --version)"

    # Install hooks
    log_info "Installing git hooks..."
    "${PRE_COMMIT[@]}" install

    # Also install commit-msg hook for conventional commits (optional)
    # "${PRE_COMMIT[@]}" install --hook-type commit-msg

    log_info "Hooks installed successfully!"
    echo ""
    echo "Pre-commit will now run automatically on git commit."
    echo ""
    echo "Useful commands:"
    echo "  ${PRE_COMMIT[*]} run --all-files    # Run all hooks on all files"
    echo "  ${PRE_COMMIT[*]} run clang-format   # Run specific hook"
    echo "  git commit --no-verify        # Skip hooks (use sparingly)"
}

# Remove git hooks
remove_hooks() {
    log_header "Removing Pre-commit Hooks"

    cd "$REPO_ROOT"

    if resolve_precommit; then
        "${PRE_COMMIT[@]}" uninstall
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
  - clang-format           Format C code (Core/User/)
  - cppcheck               Static analysis (skipped if not installed)
  - check-bench-flags      Block BMS bench-mode overrides from shipping
  - auto-bump-patch        Bump patch VERSION of affected boards + repo

pre-commit is installed via Homebrew, pipx, or 'python -m pip install --user'
(first that works). It does not need to be on PATH afterwards: if the
'pre-commit' command isn't found, 'python -m pre_commit' is used instead, and
the installed git hook calls Python directly.

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
