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

# Available boards
BOARDS=("BMS" "DASH" "DART" "DCU" "LVPDB" "PCU" "Sensor_Nodes" "UART_TEST")

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

# Get the script's directory
get_script_dir() {
    cd "$(dirname "$0")" && pwd
}

# Get the build directory (for repo mode)
get_build_dir() {
    echo "$(get_script_dir)/../build/Debug"
}

# Check if running in standalone mode (release package, not in repo)
is_standalone_mode() {
    local build_dir="$(get_build_dir)"
    # If build directory doesn't exist, check for firmware files nearby
    if [ ! -d "$build_dir" ]; then
        local script_dir="$(get_script_dir)"
        # Look for .elf files in script dir or board subdirs
        if ls "$script_dir"/*.elf >/dev/null 2>&1 || ls "$script_dir"/*/*.elf >/dev/null 2>&1; then
            return 0
        fi
    fi
    return 1
}

# Discover firmware file for a board in standalone mode
discover_firmware() {
    local board="$1"
    local script_dir="$(get_script_dir)"
    local elf_file=""

    # Check board subdirectory first (e.g., ./BMS/BMS-latest-*.elf)
    if [ -d "$script_dir/$board" ]; then
        elf_file=$(ls "$script_dir/$board"/*.elf 2>/dev/null | head -1)
    fi

    # Fall back to flat structure (e.g., ./BMS-latest-*.elf)
    if [ -z "$elf_file" ]; then
        elf_file=$(ls "$script_dir/$board"-*.elf 2>/dev/null | head -1)
    fi

    echo "$elf_file"
}

# Get the .elf file path for a board (works in both repo and standalone mode)
get_elf_path() {
    local board="$1"

    if is_standalone_mode; then
        discover_firmware "$board"
    else
        local build_dir="$(get_build_dir)"
        echo "$build_dir/$board/$board.elf"
    fi
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

# Patch an ELF with flash-time metadata using scripts/flash-patcher.py.
# On success, prints the patched ELF path to stdout and the metadata to
# stderr. On failure (patcher missing, section absent, magic mismatch on
# an older build) echoes the original path so the caller still flashes
# something. board_name is optional - used to name the patched output.
patch_elf_for_flash() {
    local src_elf="$1"
    local board_name="$2"

    if [[ "$src_elf" != *.elf ]]; then
        echo "$src_elf"
        return 0
    fi

    local script_dir
    script_dir="$(get_script_dir)"
    local patcher="$script_dir/flash-patcher.py"

    if [ ! -x "$patcher" ]; then
        log_warn "flash-patcher.py not found at $patcher - flashing without metadata stamp"
        echo "$src_elf"
        return 0
    fi

    if ! command -v python3 &> /dev/null; then
        log_warn "python3 not found - flashing without metadata stamp"
        echo "$src_elf"
        return 0
    fi

    local patched_elf
    if [ -n "$board_name" ]; then
        patched_elf="${src_elf%.elf}.patched.elf"
    else
        patched_elf="${src_elf%.elf}.patched.elf"
    fi

    # Capture metadata for logging via --print mode. --print emits key=value
    # lines on stdout; human-readable progress goes to stderr via the logger.
    local metadata
    if ! metadata=$(python3 "$patcher" --elf "$src_elf" --out "$patched_elf" --print 2>/dev/null); then
        log_warn "flash-patcher failed - flashing original ELF unpatched"
        echo "$src_elf"
        return 0
    fi

    # Echo metadata back through stderr for human visibility. Keys are
    # flash_utc / flasher_user / flasher_host / elf (from --print).
    log_info "Stamped flash metadata:"
    while IFS='=' read -r k v; do
        case "$k" in
            flash_utc|flasher_user|flasher_host)
                printf "  %-14s %s\n" "$k" "$v" >&2
                ;;
        esac
    done <<< "$metadata"

    # Stash the metadata for record_flash to pick up without re-running.
    FEB_LAST_FLASH_METADATA="$metadata"
    echo "$patched_elf"
}

# Append a row to build/flash_history.csv after a successful flash.
# Silent on missing build dir (standalone mode).
record_flash() {
    local board_name="$1"
    local elf_file="$2"
    local exit_code="$3"

    # Only log successful flashes - failures are noise.
    if [ "$exit_code" -ne 0 ]; then
        return 0
    fi

    local script_dir
    script_dir="$(get_script_dir)"
    local build_dir
    build_dir="$(get_build_dir)"

    # Skip logging if we have no writable build directory (standalone).
    if [ ! -d "$build_dir" ]; then
        return 0
    fi

    local history="$build_dir/../flash_history.csv"
    local flash_utc="" flasher_user="" flasher_host=""
    if [ -n "${FEB_LAST_FLASH_METADATA:-}" ]; then
        while IFS='=' read -r k v; do
            case "$k" in
                flash_utc)    flash_utc="$v" ;;
                flasher_user) flasher_user="$v" ;;
                flasher_host) flasher_host="$v" ;;
            esac
        done <<< "$FEB_LAST_FLASH_METADATA"
    fi
    if [ -z "$flash_utc" ]; then
        flash_utc=$(date -u "+%Y-%m-%dT%H:%M:%SZ")
    fi
    if [ -z "$flasher_user" ]; then
        flasher_user="${USER:-unknown}"
    fi
    if [ -z "$flasher_host" ]; then
        flasher_host=$(hostname -s 2>/dev/null || echo "unknown")
    fi

    # Write header on first append.
    if [ ! -f "$history" ]; then
        echo "flash_utc,board,elf,flasher_user,flasher_host" > "$history"
    fi
    echo "${flash_utc},${board_name:-unknown},${elf_file},${flasher_user},${flasher_host}" >> "$history"
    log_info "Recorded flash in $(basename "$history")"
}

# Flash a target file
flash_target() {
    local target_file="$1"
    local board_name="${2:-}"

    if [ ! -f "$target_file" ]; then
        log_error "File not found: $target_file"
        return 1
    fi

    # Reset metadata cache; patch_elf_for_flash populates it.
    FEB_LAST_FLASH_METADATA=""

    # Stamp flash-time provenance into the ELF (falls back to original
    # on any failure so legacy/missing-section ELFs still flash).
    local flash_file
    flash_file=$(patch_elf_for_flash "$target_file" "$board_name")

    log_info "Flashing: $flash_file"
    echo ""

    STM32_Programmer_CLI --connect port=swd --download "$flash_file" -hardRst -rst --start

    local exit_code=$?
    echo ""

    if [ $exit_code -eq 0 ]; then
        log_info "Flash completed successfully!"
        record_flash "$board_name" "$target_file" $exit_code
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

    if [ -z "$elf_path" ] || [ ! -f "$elf_path" ]; then
        if is_standalone_mode; then
            log_error "Firmware not found for $board"
            echo "Make sure the firmware files are in the correct location."
            return 1
        else
            log_warn "Firmware not found: $elf_path"
            echo ""
            read -p "Would you like to build $board first? [y/N] " -n 1 -r
            echo ""

            if [[ $REPLY =~ ^[Yy]$ ]]; then
                log_info "Building $board..."
                cmake --build "$(get_build_dir)" --target "$board"
                echo ""
                elf_path=$(get_elf_path "$board")
            else
                log_error "Cannot flash without firmware. Build first with:"
                echo "  cmake --build build/Debug --target $board"
                return 1
            fi
        fi
    fi

    flash_target "$elf_path" "$board"
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

    local standalone=false
    if is_standalone_mode; then
        standalone=true
    fi

    for i in "${!BOARDS[@]}"; do
        local board="${BOARDS[$i]}"
        local elf_path
        elf_path=$(get_elf_path "$board")

        local status=""
        if [ -n "$elf_path" ] && [ -f "$elf_path" ]; then
            local build_time
            build_time=$(get_file_time "$elf_path")
            status="${GREEN}[built ${build_time}]${NC}"
        else
            if [ "$standalone" = true ]; then
                status="${RED}[not found]${NC}"
            else
                status="${YELLOW}[not built]${NC}"
            fi
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
        [1-8])
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
