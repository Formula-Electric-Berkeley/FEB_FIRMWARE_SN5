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

# Step 1: Check toolchain
check_toolchain() {
    log_header "Step 1/5: Checking Toolchain"

    local all_good=true

    # ARM GCC
    if command_exists arm-none-eabi-gcc; then
        local version
        version=$(arm-none-eabi-gcc --version | head -1)
        echo -e "  ${GREEN}✓${NC} ARM GCC: $version"
    else
        echo -e "  ${RED}✗${NC} ARM GCC: not found"
        all_good=false
    fi

    # CMake
    if command_exists cmake; then
        local version
        version=$(cmake --version | head -1)
        echo -e "  ${GREEN}✓${NC} CMake: $version"
    else
        echo -e "  ${RED}✗${NC} CMake: not found"
        all_good=false
    fi

    # Ninja
    if command_exists ninja; then
        local version
        version=$(ninja --version)
        echo -e "  ${GREEN}✓${NC} Ninja: $version"
    else
        echo -e "  ${RED}✗${NC} Ninja: not found"
        all_good=false
    fi

    # Git
    if command_exists git; then
        local version
        version=$(git --version)
        echo -e "  ${GREEN}✓${NC} Git: $version"
    else
        echo -e "  ${RED}✗${NC} Git: not found"
        all_good=false
    fi

    # Optional tools
    if command_exists clang-format; then
        echo -e "  ${GREEN}✓${NC} clang-format: $(clang-format --version | head -1)"
    else
        echo -e "  ${YELLOW}○${NC} clang-format: not found (optional)"
    fi

    if command_exists STM32_Programmer_CLI; then
        echo -e "  ${GREEN}✓${NC} STM32_Programmer_CLI: available"
    else
        echo -e "  ${YELLOW}○${NC} STM32_Programmer_CLI: not found (needed for flashing)"
    fi

    echo ""

    if [ "$all_good" = false ]; then
        log_error "Missing required tools."
        echo ""

        # On Windows, try to auto-configure PATH
        case "$(uname -s)" in
            MINGW*|MSYS*|CYGWIN*)
                if configure_windows_path; then
                    echo ""
                    log_info "Retrying tool detection..."
                    echo ""
                    # Re-check after PATH update (recursive call)
                    check_toolchain
                    return $?
                fi
                ;;
        esac

        show_install_instructions
        return 1
    fi

    log_info "All required tools are installed"
}

show_install_instructions() {
    case "$(uname -s)" in
        Darwin*)
            echo -e "${BOLD}macOS Installation:${NC}"
            echo ""
            echo "  # Required tools"
            echo "  brew install cmake ninja"
            echo "  brew install --cask gcc-arm-embedded"
            echo ""
            echo "  # Optional tools"
            echo "  brew install clang-format"
            echo ""
            echo "  # For flashing (STM32CubeCLT)"
            echo "  Download from: https://www.st.com/en/development-tools/stm32cubeclt.html"
            ;;
        Linux*)
            echo -e "${BOLD}Linux Installation:${NC}"
            echo ""
            echo "  # Required tools"
            echo "  sudo apt update"
            echo "  sudo apt install cmake ninja-build gcc-arm-none-eabi"
            echo ""
            echo "  # Optional tools"
            echo "  sudo apt install clang-format"
            echo ""
            echo "  # For flashing (STM32CubeCLT)"
            echo "  Download from: https://www.st.com/en/development-tools/stm32cubeclt.html"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo -e "${BOLD}Windows Installation:${NC}"
            echo ""
            echo "  Install STM32CubeCLT (bundles all required tools):"
            echo "  https://www.st.com/en/development-tools/stm32cubeclt.html"
            echo ""
            echo "  Default install location: C:\\ST\\STM32CubeCLT"
            echo ""
            echo "  After installing, re-run this setup script to configure PATH."
            ;;
    esac
}

# Configure PATH for Windows Git Bash
configure_windows_path() {
    local cubeclt_path="/c/ST/STM32CubeCLT"
    local bashrc="$HOME/.bashrc"

    # Check if STM32CubeCLT exists at default location
    if [ ! -d "$cubeclt_path" ]; then
        log_warn "STM32CubeCLT not found at $cubeclt_path"
        echo "  Please install STM32CubeCLT first."
        return 1
    fi

    # Check if already configured
    if grep -q "STM32CubeCLT" "$bashrc" 2>/dev/null; then
        log_info "PATH already configured in ~/.bashrc"
        echo "  Try running: source ~/.bashrc"
        return 1
    fi

    echo ""
    log_info "Found STM32CubeCLT at $cubeclt_path"
    echo ""
    echo "  The following will be added to ~/.bashrc:"
    echo ""
    echo "    export PATH=\"$cubeclt_path/GNU-tools-for-STM32/bin:\$PATH\""
    echo "    export PATH=\"$cubeclt_path/CMake/bin:\$PATH\""
    echo "    export PATH=\"$cubeclt_path/Ninja/bin:\$PATH\""
    echo "    export PATH=\"$cubeclt_path/STM32CubeProgrammer/bin:\$PATH\""
    echo "    export CUBE_BUNDLE_PATH=\"$cubeclt_path\""
    echo ""

    read -p "Add to ~/.bashrc? [Y/n] " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Nn]$ ]]; then
        log_warn "Skipping PATH configuration. You'll need to configure manually."
        return 1
    fi

    # Create ~/.bash_profile if it doesn't exist (Git Bash sources this for login shells)
    if [ ! -f "$HOME/.bash_profile" ]; then
        log_step "Creating ~/.bash_profile to source ~/.bashrc..."
        cat > "$HOME/.bash_profile" << 'PROFILE_EOF'
# Load .bashrc if it exists
if [ -f "$HOME/.bashrc" ]; then
    source "$HOME/.bashrc"
fi
PROFILE_EOF
    fi

    # Append to bashrc
    cat >> "$bashrc" << BASHRC_EOF

# STM32CubeCLT tools (added by FEB setup script)
export PATH="$cubeclt_path/GNU-tools-for-STM32/bin:\$PATH"
export PATH="$cubeclt_path/CMake/bin:\$PATH"
export PATH="$cubeclt_path/Ninja/bin:\$PATH"
export PATH="$cubeclt_path/STM32CubeProgrammer/bin:\$PATH"
export CUBE_BUNDLE_PATH="$cubeclt_path"
BASHRC_EOF

    log_info "PATH configuration added to ~/.bashrc"
    echo ""

    # Source it now so setup can continue
    source "$bashrc"
    log_info "PATH updated for current session"
    return 0
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
        read -p "Reconfigure? [y/N] " -n 1 -r
        echo ""
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
    echo "  4. Create a release version:"
    echo "     ./scripts/version.sh patch"
    echo ""
    echo -e "${BOLD}Documentation:${NC}"
    echo "  - README.md        General project info"
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
  1. Check toolchain (ARM GCC, CMake, Ninja)
  2. Initialize git submodules
  3. Install pre-commit hooks (if configured)
  4. Configure CMake
  5. Run initial build (optional with --quick)

Prerequisites:
  - ARM GCC toolchain (arm-none-eabi-gcc)
  - CMake >= 3.22
  - Ninja build system
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
