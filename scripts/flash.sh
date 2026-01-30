#!/bin/bash
#
# STM32 Board Flash Script
#
# Usage:
#   ./scripts/flash.sh                      # Interactive menu to select board
#   ./scripts/flash.sh -b LVPDB             # Flash specific board
#   ./scripts/flash.sh -f path/to/file.elf  # Flash specific file
#   ./scripts/flash.sh -l                   # Loop mode (keep flashing)
#   ./scripts/flash.sh --list-probes        # List connected SWD probes
#   ./scripts/flash.sh -h                   # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."
BUILD_DIR="$REPO_ROOT/build/Debug"

# Available boards
BOARDS=("BMS" "DASH" "DART" "DCU" "LVPDB" "PCU" "Sensor_Nodes")

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

# Check if STM32_Programmer_CLI is available
check_prerequisites() {
    if command -v STM32_Programmer_CLI &> /dev/null; then
        return 0
    fi

    # Check common installation paths
    local common_paths=(
        "/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI"
        "/opt/st/stm32cubeclt/STM32CubeProgrammer/bin/STM32_Programmer_CLI"
        "$HOME/STM32CubeCLT/STM32CubeProgrammer/bin/STM32_Programmer_CLI"
        "/usr/local/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI"
    )

    for path in "${common_paths[@]}"; do
        if [ -x "$path" ]; then
            log_warn "Found STM32_Programmer_CLI at: $path"
            log_warn "Add it to your PATH for convenience."
            export PATH="$(dirname "$path"):$PATH"
            return 0
        fi
    done

    return 1
}

# Show setup instructions for installing STM32CubeCLT
show_setup_instructions() {
    log_header "STM32CubeCLT Setup Required"

    echo -e "${BOLD}STM32_Programmer_CLI is not installed or not in PATH.${NC}"
    echo ""
    echo "To flash STM32 boards, you need to install STM32CubeCLT (Command Line Tools)."
    echo ""

    # Detect platform
    case "$(uname -s)" in
        Darwin*)
            echo -e "${CYAN}macOS Installation:${NC}"
            echo "  1. Download STM32CubeCLT from:"
            echo "     https://www.st.com/en/development-tools/stm32cubeclt.html"
            echo ""
            echo "  2. Run the installer package (.pkg file)"
            echo ""
            echo "  3. Add to your PATH by adding this to ~/.zshrc or ~/.bash_profile:"
            echo "     export PATH=\"/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin:\$PATH\""
            echo ""
            echo "  4. Restart your terminal or run: source ~/.zshrc"
            ;;
        Linux*)
            echo -e "${CYAN}Linux Installation:${NC}"
            echo "  1. Download STM32CubeCLT from:"
            echo "     https://www.st.com/en/development-tools/stm32cubeclt.html"
            echo ""
            echo "  2. Extract and run the installer:"
            echo "     chmod +x st-stm32cubeclt_*.sh"
            echo "     sudo ./st-stm32cubeclt_*.sh"
            echo ""
            echo "  3. Add to your PATH by adding this to ~/.bashrc:"
            echo "     export PATH=\"/opt/st/stm32cubeclt/STM32CubeProgrammer/bin:\$PATH\""
            echo ""
            echo "  4. Restart your terminal or run: source ~/.bashrc"
            echo ""
            echo "  Note: You may need to install udev rules for USB access:"
            echo "     sudo cp /opt/st/stm32cubeclt/STM32CubeProgrammer/Drivers/rules/*.rules /etc/udev/rules.d/"
            echo "     sudo udevadm control --reload-rules"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo -e "${CYAN}Windows Installation:${NC}"
            echo "  1. Download STM32CubeCLT from:"
            echo "     https://www.st.com/en/development-tools/stm32cubeclt.html"
            echo ""
            echo "  2. Run the installer (.exe file)"
            echo ""
            echo "  3. The installer should add STM32_Programmer_CLI to PATH automatically."
            echo "     If not, add this to your system PATH:"
            echo "     C:\\ST\\STM32CubeCLT\\STM32CubeProgrammer\\bin"
            echo ""
            echo "  4. Restart your terminal"
            ;;
        *)
            echo "  Download STM32CubeCLT from:"
            echo "  https://www.st.com/en/development-tools/stm32cubeclt.html"
            ;;
    esac

    echo ""
    echo -e "${YELLOW}After installation, run this script again.${NC}"
}

# Get the .elf file path for a board
get_elf_path() {
    local board="$1"
    echo "$BUILD_DIR/$board/$board.elf"
}

# Check if a board name is valid
is_valid_board() {
    local board="$1"
    for b in "${BOARDS[@]}"; do
        if [ "$b" = "$board" ]; then
            return 0
        fi
    done
    return 1
}

# Flash a target file
flash_target() {
    local target_file="$1"

    if [ ! -f "$target_file" ]; then
        log_error "File not found: $target_file"
        return 1
    fi

    log_info "Flashing: $target_file"
    echo ""

    STM32_Programmer_CLI --connect port=swd --download "$target_file" -hardRst -rst --start

    local exit_code=$?
    echo ""

    if [ $exit_code -eq 0 ]; then
        log_info "Flash completed successfully!"
    else
        log_error "Flash failed with exit code: $exit_code"
    fi

    return $exit_code
}

# Flash a board by name
flash_board() {
    local board="$1"

    if ! is_valid_board "$board"; then
        log_error "Invalid board name: $board"
        echo "Valid boards: ${BOARDS[*]}"
        return 1
    fi

    local elf_path
    elf_path=$(get_elf_path "$board")

    if [ ! -f "$elf_path" ]; then
        log_warn "Firmware not found: $elf_path"
        echo ""
        read -p "Would you like to build $board first? [y/N] " -n 1 -r
        echo ""

        if [[ $REPLY =~ ^[Yy]$ ]]; then
            log_info "Building $board..."
            cmake --build "$BUILD_DIR" --target "$board"
            echo ""
        else
            log_error "Cannot flash without firmware. Build first with:"
            echo "  cmake --build build/Debug --target $board"
            return 1
        fi
    fi

    flash_target "$elf_path"
}

# Get file modification time (cross-platform)
get_file_time() {
    local file="$1"
    if [[ "$(uname -s)" == "Darwin" ]]; then
        stat -f "%Sm" -t "%Y-%m-%d %H:%M" "$file"
    else
        date -r "$file" "+%Y-%m-%d %H:%M"
    fi
}

# Show board selection menu
show_board_menu() {
    echo -e "${BOLD}Select a board to flash:${NC}"
    echo ""

    for i in "${!BOARDS[@]}"; do
        local board="${BOARDS[$i]}"
        local elf_path
        elf_path=$(get_elf_path "$board")

        local status=""
        if [ -f "$elf_path" ]; then
            local build_time
            build_time=$(get_file_time "$elf_path")
            status="${GREEN}[built ${build_time}]${NC}"
        else
            status="${YELLOW}[not built]${NC}"
        fi

        printf "  %d) %-15s %b\n" $((i + 1)) "$board" "$status"
    done

    echo ""
    echo "  q) Quit"
    echo ""
}

# Interactive board selection
interactive_select() {
    show_board_menu

    read -p "Enter selection: " selection

    case "$selection" in
        q|Q|quit|exit)
            return 1
            ;;
        [1-7])
            local index=$((selection - 1))
            if [ $index -ge 0 ] && [ $index -lt ${#BOARDS[@]} ]; then
                flash_board "${BOARDS[$index]}"
                return $?
            fi
            ;;
        *)
            # Check if it's a board name
            if is_valid_board "$selection"; then
                flash_board "$selection"
                return $?
            fi

            # Check if it's a file path
            if [ -f "$selection" ]; then
                flash_target "$selection"
                return $?
            fi

            log_error "Invalid selection: $selection"
            return 1
            ;;
    esac
}

# Loop mode - keep flashing
loop_mode() {
    log_header "Loop Mode"
    echo "Flash multiple boards in sequence. Enter 'q' to quit."
    echo ""

    while true; do
        if ! interactive_select; then
            break
        fi
        echo ""
    done

    log_info "Exiting loop mode."
}

# List connected probes
list_probes() {
    log_header "Connected SWD Probes"
    STM32_Programmer_CLI --list
}

# Show help
show_help() {
    echo "FEB Firmware Flash Script"
    echo ""
    echo "Usage: ./scripts/flash.sh [options]"
    echo ""
    echo "Options:"
    echo "  -b, --board <BOARD>    Flash specified board (BMS, DASH, DART, DCU, LVPDB, PCU, Sensor_Nodes)"
    echo "  -f, --file <PATH>      Flash a specific .elf/.bin/.hex file"
    echo "  -l, --loop             Loop mode: keep prompting for boards to flash"
    echo "      --list-probes      List connected SWD probes"
    echo "  -h, --help             Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./scripts/flash.sh                    # Interactive menu"
    echo "  ./scripts/flash.sh -b LVPDB           # Flash LVPDB board"
    echo "  ./scripts/flash.sh -f build/Debug/LVPDB/LVPDB.elf"
    echo "  ./scripts/flash.sh -l                 # Flash multiple boards"
    echo "  ./scripts/flash.sh --list-probes      # Check connected programmers"
    echo ""
    echo "Available boards:"
    echo "  ${BOARDS[*]}"
    echo ""
    echo "Prerequisites:"
    echo "  - STM32CubeCLT (provides STM32_Programmer_CLI)"
    echo "  - SWD programmer connected (ST-Link, J-Link, etc.)"
}

# Main function
main() {
    local board=""
    local file=""
    local loop=false
    local list_probes_flag=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -b|--board)
                board="$2"
                shift 2
                ;;
            -f|--file)
                file="$2"
                shift 2
                ;;
            -l|--loop)
                loop=true
                shift
                ;;
            --list-probes)
                list_probes_flag=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                echo ""
                show_help
                exit 1
                ;;
        esac
    done

    # Check prerequisites
    if ! check_prerequisites; then
        show_setup_instructions
        exit 1
    fi

    # Handle --list-probes
    if [ "$list_probes_flag" = true ]; then
        list_probes
        exit 0
    fi

    log_header "FEB Firmware Flash"

    # Determine what to flash
    if [ -n "$file" ]; then
        # Flash specific file
        flash_target "$file"
    elif [ -n "$board" ]; then
        # Flash specific board
        flash_board "$board"
        if [ "$loop" = true ]; then
            echo ""
            loop_mode
        fi
    elif [ "$loop" = true ]; then
        # Loop mode from start
        loop_mode
    else
        # Interactive mode
        interactive_select
    fi
}

main "$@"
