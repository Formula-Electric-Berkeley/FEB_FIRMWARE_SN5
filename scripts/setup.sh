#!/bin/bash
#
# FEB Firmware First-Time Setup Script
#
# Sets up the development environment for new developers:
# - Verifies/guides toolchain installation
# - Initializes git submodules
# - Installs pre-commit hooks (if available)
# - Configures CMake
# - Runs initial build to verify everything works
#
# Usage:
#   ./scripts/setup.sh           # Full setup
#   ./scripts/setup.sh --quick   # Skip initial build
#   ./scripts/setup.sh -h        # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

source "$SCRIPT_DIR/lib/cube-tools.sh"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
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

log_step() {
    echo -e "${CYAN}→${NC} $1"
}

# Check if a command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Prompt with a default value when stdin is not a TTY (for CI / non-interactive).
# Usage: prompt_yes_no "Question?" "Y"  -> sets REPLY based on user input or default.
prompt_yes_no() {
    local question="$1"
    local default="${2:-N}"
    if [ ! -t 0 ]; then
        REPLY="$default"
        echo "$question [auto: $default]"
        return 0
    fi
    read -r -p "$question " -n 1
    echo ""
}

# Step 1: Check toolchain
check_toolchain() {
    log_header "Step 1/5: Checking Toolchain"

    if ! feb_find_cube; then
        echo -e "  ${RED}✗${NC} cube: not found"
        echo ""
        feb_print_cube_install_hint
        return 1
    fi
    echo -e "  ${GREEN}✓${NC} cube: $FEB_CUBE"

    log_step "Installing bundles pinned in .settings/bundles.store.json..."
    if ! (cd "$REPO_ROOT" && "$FEB_CUBE" bundle install --project --yes); then
        log_error "cube bundle install failed"
        return 1
    fi

    if ! feb_use_cube_tools "$REPO_ROOT" cmake programmer; then
        log_error "cube could not resolve the pinned CMake/programmer bundles"
        return 1
    fi

    local all_good=true
    local tool
    for tool in arm-none-eabi-gcc cmake ninja git; do
        if command_exists "$tool"; then
            echo -e "  ${GREEN}✓${NC} $tool: $("$tool" --version | head -1)"
        else
            echo -e "  ${RED}✗${NC} $tool: not found"
            all_good=false
        fi
    done

    if command_exists STM32_Programmer_CLI; then
        echo -e "  ${GREEN}✓${NC} STM32_Programmer_CLI: available"
    else
        echo -e "  ${YELLOW}○${NC} STM32_Programmer_CLI: not found (needed for flashing)"
    fi

    if command_exists clang-format; then
        echo -e "  ${GREEN}✓${NC} clang-format: $(clang-format --version | head -1)"
    else
        echo -e "  ${YELLOW}○${NC} clang-format: not found (optional)"
    fi

    echo ""

    if [ "$all_good" = false ]; then
        log_error "Missing required tools."
        return 1
    fi

    log_info "All required tools are installed"
}

# Step 2: Initialize submodules
init_submodules() {
    log_header "Step 2/5: Initializing Git Submodules"

    cd "$REPO_ROOT"

    if [ -f ".gitmodules" ]; then
        log_step "Updating submodules..."
        git submodule update --init --recursive

        # Show submodule status
        echo ""
        git submodule status
        echo ""
        log_info "Submodules initialized"
    else
        log_info "No submodules found"
    fi
}

# Step 3: Install pre-commit hooks
install_hooks() {
    log_header "Step 3/5: Setting Up Pre-commit Hooks"

    cd "$REPO_ROOT"

    if [ -f "scripts/setup-hooks.sh" ]; then
        # Use the dedicated setup script (handles Homebrew, pipx, etc.)
        log_step "Running setup-hooks.sh..."
        bash scripts/setup-hooks.sh
    elif [ -f ".pre-commit-config.yaml" ]; then
        if command_exists pre-commit; then
            log_step "Installing pre-commit hooks..."
            pre-commit install
            log_info "Pre-commit hooks installed"
        else
            log_warn "pre-commit not found. Skipping hook installation."
            echo "  Install with: brew install pre-commit (macOS)"
            echo "             or: pipx install pre-commit"
        fi
    else
        log_info "No pre-commit configuration found. Skipping."
    fi
}

# Step 4: Configure CMake
configure_cmake() {
    log_header "Step 4/5: Configuring CMake"

    cd "$REPO_ROOT"

    if [ -d "build/Debug" ] && [ -f "build/Debug/build.ninja" ]; then
        log_info "CMake already configured (build/Debug exists)"
        prompt_yes_no "Reconfigure? [y/N]" "N"
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            return 0
        fi
    fi

    log_step "Configuring Debug build..."
    cmake --preset Debug

    log_info "CMake configured successfully"
}

# Step 5: Initial build
initial_build() {
    log_header "Step 5/5: Initial Build"

    cd "$REPO_ROOT"

    log_step "Building all boards (this may take a few minutes)..."
    echo ""

    if cmake --build build/Debug; then
        log_info "Build successful!"
    else
        log_error "Build failed. Check the errors above."
        return 1
    fi
}

# Show final summary
show_summary() {
    log_header "Setup Complete!"

    echo -e "${BOLD}Next steps:${NC}"
    echo ""
    echo "  1. Build a specific board:"
    echo "     ./scripts/build.sh LVPDB"
    echo ""
    echo "  2. Flash firmware to a board:"
    echo "     ./scripts/flash.sh -b LVPDB"
    echo ""
    echo "  3. Format code before committing:"
    echo "     ./scripts/format.sh"
    echo ""
    echo "  4. Publish a release (CalVer date tag):"
    echo "     git tag v\$(date -u +%Y.%m.%d) && git push origin v\$(date -u +%Y.%m.%d)"
    echo ""
    echo -e "${BOLD}Documentation:${NC}"
    echo "  - README.md                    Repo overview, build, flash, CI"
    echo "  - <BOARD>/README.md            Per-board docs (e.g., BMS/README.md)"
    echo "  - common/README.md             Shared library index"
    echo "  - common/<LIB>/README.md       Per-library docs (e.g., common/FEB_Time_Library/README.md)"
    echo "  - scripts/README.md            Script catalog"
    echo ""
    echo -e "${GREEN}Happy coding!${NC}"
}

# Show help
show_help() {
    cat << EOF
FEB Firmware First-Time Setup Script

Sets up the development environment for new developers.

Usage:
  ./scripts/setup.sh           Full setup (includes initial build)
  ./scripts/setup.sh --quick   Skip initial build
  ./scripts/setup.sh -h        Show this help

Steps performed:
  1. Install the toolchain bundles pinned in .settings/bundles.store.json
     (CMake, Ninja, GNU tools for STM32, STM32CubeProgrammer) via 'cube'
  2. Initialize git submodules
  3. Install pre-commit hooks (if configured)
  4. Configure CMake
  5. Run initial build (optional with --quick)

Prerequisites:
  - STM32Cube for Visual Studio Code extension (provides the 'cube' CLI)
  - Git
EOF
}

# Main function
main() {
    local quick=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --quick)
                quick=true
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

    echo ""
    echo -e "${BOLD}╔══════════════════════════════════════╗${NC}"
    echo -e "${BOLD}║    FEB Firmware Development Setup    ║${NC}"
    echo -e "${BOLD}╚══════════════════════════════════════╝${NC}"

    # Run setup steps
    if ! check_toolchain; then
        exit 1
    fi

    init_submodules
    install_hooks
    configure_cmake

    if [ "$quick" = false ]; then
        initial_build
    else
        log_header "Step 5/5: Initial Build"
        log_info "Skipped (--quick mode)"
    fi

    show_summary
}

main "$@"
