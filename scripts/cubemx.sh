#!/bin/bash
#
# STM32CubeMX IOC Management Script
#
# Generate HAL code from .ioc files without the STM32CubeMX GUI.
#
# Usage:
#   ./scripts/cubemx.sh                     # Interactive menu
#   ./scripts/cubemx.sh -g -b BMS           # Generate code for BMS
#   ./scripts/cubemx.sh -i -b LVPDB         # Inspect LVPDB configuration
#   ./scripts/cubemx.sh -a -g               # Generate code for all boards
#   ./scripts/cubemx.sh --list-boards       # List all boards with .ioc status
#   ./scripts/cubemx.sh -h                  # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

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

# STM32CubeMX path (set by check_cubemx)
CUBEMX_PATH=""

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

# Get .ioc file path for a board
get_ioc_path() {
    local board="$1"
    echo "$REPO_ROOT/$board/$board.ioc"
}

# Check if board name is valid
is_valid_board() {
    local board="$1"
    for b in "${BOARDS[@]}"; do
        [ "$b" = "$board" ] && return 0
    done
    return 1
}

# Check if .ioc file exists for board
has_ioc_file() {
    local board="$1"
    [ -f "$(get_ioc_path "$board")" ]
}

# Read a specific key from .ioc file
ioc_get_value() {
    local ioc_file="$1"
    local key="$2"
    grep "^${key}=" "$ioc_file" 2>/dev/null | cut -d'=' -f2- | sed 's/\\:/:/g'
}

# Check if STM32CubeMX is available
check_cubemx() {
    # Check if already in PATH
    if command -v STM32CubeMX &> /dev/null; then
        CUBEMX_PATH="STM32CubeMX"
        return 0
    fi

    # Common installation paths by platform
    local common_paths=()

    case "$(uname -s)" in
        Darwin*)
            common_paths=(
                "/Applications/STMicroelectronics/STM32CubeMX.app/Contents/MacOs/STM32CubeMX"
                "/Applications/STM32CubeMX.app/Contents/MacOs/STM32CubeMX"
                "$HOME/Applications/STM32CubeMX.app/Contents/MacOs/STM32CubeMX"
            )
            ;;
        Linux*)
            common_paths=(
                "/opt/st/stm32cubemx/STM32CubeMX"
                "/usr/local/STMicroelectronics/STM32CubeMX/STM32CubeMX"
                "$HOME/STM32CubeMX/STM32CubeMX"
            )
            ;;
        MINGW*|MSYS*|CYGWIN*)
            common_paths=(
                "/c/Program Files/STMicroelectronics/STM32Cube/STM32CubeMX/STM32CubeMX.exe"
                "/c/ST/STM32CubeMX/STM32CubeMX.exe"
            )
            ;;
    esac

    for path in "${common_paths[@]}"; do
        if [ -x "$path" ]; then
            CUBEMX_PATH="$path"
            log_info "Found STM32CubeMX at: $path"
            return 0
        fi
    done

    return 1
}

# Show setup instructions for installing STM32CubeMX
show_cubemx_install_instructions() {
    log_header "STM32CubeMX Setup Required"

    echo -e "${BOLD}STM32CubeMX is not installed or not found.${NC}"
    echo ""
    echo "Download from:"
    echo "  https://www.st.com/en/development-tools/stm32cubemx.html"
    echo ""

    case "$(uname -s)" in
        Darwin*)
            echo -e "${CYAN}macOS:${NC}"
            echo "  1. Download the .zip installer"
            echo "  2. Extract and run the installer"
            echo "  3. Default location: /Applications/STMicroelectronics/STM32CubeMX.app"
            ;;
        Linux*)
            echo -e "${CYAN}Linux:${NC}"
            echo "  1. Download the Linux installer (.linux file)"
            echo "  2. Make executable: chmod +x SetupSTM32CubeMX-*.linux"
            echo "  3. Run installer: sudo ./SetupSTM32CubeMX-*.linux"
            echo "  4. Default location: /opt/st/stm32cubemx/"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo -e "${CYAN}Windows:${NC}"
            echo "  1. Download the Windows installer (.exe)"
            echo "  2. Run the installer"
            echo "  3. Default location: C:\\Program Files\\STMicroelectronics\\STM32Cube\\STM32CubeMX"
            ;;
    esac

    echo ""
    echo -e "${YELLOW}After installation, run this script again.${NC}"
}

# Inspect .ioc configuration
inspect_ioc() {
    local board="$1"
    local ioc_file
    ioc_file=$(get_ioc_path "$board")

    if [ ! -f "$ioc_file" ]; then
        log_error "IOC file not found: $ioc_file"
        return 1
    fi

    log_header "$board Configuration Summary"

    # MCU Info
    echo -e "${BOLD}MCU:${NC}"
    echo "  Name:    $(ioc_get_value "$ioc_file" "Mcu.Name")"
    echo "  Package: $(ioc_get_value "$ioc_file" "Mcu.Package")"
    echo "  Family:  $(ioc_get_value "$ioc_file" "Mcu.Family")"
    echo ""

    # Project Info
    echo -e "${BOLD}Project:${NC}"
    echo "  Name:      $(ioc_get_value "$ioc_file" "ProjectManager.ProjectName")"
    echo "  Toolchain: $(ioc_get_value "$ioc_file" "ProjectManager.TargetToolchain")"
    echo "  Heap:      $(ioc_get_value "$ioc_file" "ProjectManager.HeapSize")"
    echo "  Stack:     $(ioc_get_value "$ioc_file" "ProjectManager.StackSize")"
    echo ""

    # Clock Info
    local sysclk hclk apb1 apb2
    sysclk=$(ioc_get_value "$ioc_file" "RCC.SYSCLKFreq_VALUE")
    hclk=$(ioc_get_value "$ioc_file" "RCC.HCLKFreq_Value")
    apb1=$(ioc_get_value "$ioc_file" "RCC.APB1Freq_Value")
    apb2=$(ioc_get_value "$ioc_file" "RCC.APB2Freq_Value")

    echo -e "${BOLD}Clock:${NC}"
    if [ -n "$sysclk" ]; then
        echo "  SYSCLK:  $(echo "scale=1; $sysclk / 1000000" | bc) MHz"
    fi
    if [ -n "$hclk" ]; then
        echo "  HCLK:    $(echo "scale=1; $hclk / 1000000" | bc) MHz"
    fi
    if [ -n "$apb1" ]; then
        echo "  APB1:    $(echo "scale=1; $apb1 / 1000000" | bc) MHz"
    fi
    if [ -n "$apb2" ]; then
        echo "  APB2:    $(echo "scale=1; $apb2 / 1000000" | bc) MHz"
    fi
    echo ""

    # Peripherals
    echo -e "${BOLD}Enabled Peripherals:${NC}"
    local ip_count
    ip_count=$(ioc_get_value "$ioc_file" "Mcu.IPNb")
    if [ -n "$ip_count" ]; then
        for i in $(seq 0 $((ip_count - 1))); do
            local ip
            ip=$(ioc_get_value "$ioc_file" "Mcu.IP$i")
            [ -n "$ip" ] && echo "  - $ip"
        done
    fi
    echo ""

    # FreeRTOS (if enabled)
    if grep -q "^FREERTOS\." "$ioc_file" 2>/dev/null; then
        echo -e "${BOLD}FreeRTOS:${NC}"
        local tasks
        tasks=$(ioc_get_value "$ioc_file" "FREERTOS.Tasks01")
        if [ -n "$tasks" ]; then
            echo "  Tasks:"
            echo "$tasks" | tr ';' '\n' | while IFS= read -r task; do
                local task_name
                task_name=$(echo "$task" | cut -d',' -f1)
                [ -n "$task_name" ] && echo "    - $task_name"
            done
        fi
        echo ""
    fi
}

# Show pin assignments
show_pins() {
    local board="$1"
    local ioc_file
    ioc_file=$(get_ioc_path "$board")

    if [ ! -f "$ioc_file" ]; then
        log_error "IOC file not found: $ioc_file"
        return 1
    fi

    log_header "$board Pin Assignments"

    # Extract pin configurations with labels (only lines with .GPIO_Label=)
    grep "^P[A-K][0-9].*\.GPIO_Label=" "$ioc_file" | while IFS= read -r line; do
        local pin label
        pin=$(echo "$line" | cut -d'.' -f1)
        label=$(echo "$line" | cut -d'=' -f2)
        printf "  %-12s %s\n" "$pin" "$label"
    done | sort
}

# Show peripherals
show_peripherals() {
    local board="$1"
    local ioc_file
    ioc_file=$(get_ioc_path "$board")

    if [ ! -f "$ioc_file" ]; then
        log_error "IOC file not found: $ioc_file"
        return 1
    fi

    log_header "$board Peripherals"

    local ip_count
    ip_count=$(ioc_get_value "$ioc_file" "Mcu.IPNb")
    if [ -n "$ip_count" ]; then
        for i in $(seq 0 $((ip_count - 1))); do
            local ip
            ip=$(ioc_get_value "$ioc_file" "Mcu.IP$i")
            [ -n "$ip" ] && echo "  - $ip"
        done
    fi
}

# On Windows Git Bash (MSYS/MINGW), STM32CubeMX.exe is a native Windows binary
# that wants native paths (C:\ST\...) in its script-load arguments and on the
# command line, not POSIX paths (/c/ST/...). Convert via cygpath when present,
# sed fallback otherwise. On macOS/Linux, prints the argument unchanged.
posix_to_native_path() {
    local p="$1"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            if command -v cygpath >/dev/null 2>&1; then
                # -m emits mixed paths (C:/ST/...) instead of -w's backslashed
                # form. STM32CubeMX accepts both, and forward slashes survive
                # heredoc interpolation without being consumed as escapes.
                # Keeps the cygpath branch consistent with the sed fallback below.
                cygpath -m "$p"
            else
                echo "$p" | sed -E 's|^/([a-zA-Z])/|\1:/|'
            fi
            ;;
        *)
            echo "$p"
            ;;
    esac
}

# Generate code from .ioc file
generate_code() {
    local board="$1"
    local ioc_file
    ioc_file=$(get_ioc_path "$board")
    local project_dir
    project_dir=$(dirname "$ioc_file")

    if [ ! -f "$ioc_file" ]; then
        log_error "IOC file not found: $ioc_file"
        return 1
    fi

    log_info "Generating code for $board..."

    # Create temporary script file for CubeMX
    local script_file
    script_file=$(mktemp)

    local ioc_for_cubemx
    local script_for_cubemx
    ioc_for_cubemx=$(posix_to_native_path "$ioc_file")
    script_for_cubemx=$(posix_to_native_path "$script_file")

    cat > "$script_file" << EOF
config load "$ioc_for_cubemx"
project generate
exit
EOF

    # Run STM32CubeMX in script mode
    if "$CUBEMX_PATH" -q "$script_for_cubemx"; then
        log_info "Code generation completed for $board"
        rm -f "$script_file"
        return 0
    else
        log_error "Code generation failed for $board"
        rm -f "$script_file"
        return 1
    fi
}

# Generate code for all boards
generate_all() {
    log_header "Generating Code for All Boards"

    local success=0
    local failed=0
    local skipped=0

    for board in "${BOARDS[@]}"; do
        if has_ioc_file "$board"; then
            echo -e "${CYAN}→${NC} Processing $board..."
            if generate_code "$board"; then
                ((success++))
            else
                ((failed++))
            fi
        else
            log_warn "Skipping $board (no .ioc file)"
            ((skipped++))
        fi
    done

    echo ""
    log_header "Summary"
    echo -e "  ${GREEN}Succeeded:${NC} $success"
    echo -e "  ${RED}Failed:${NC}    $failed"
    echo -e "  ${YELLOW}Skipped:${NC}   $skipped"
}

# Migrate .ioc file to current CubeMX version
migrate_ioc() {
    local board="$1"
    local ioc_file
    ioc_file=$(get_ioc_path "$board")

    if [ ! -f "$ioc_file" ]; then
        log_error "IOC file not found: $ioc_file"
        return 1
    fi

    local old_version
    old_version=$(ioc_get_value "$ioc_file" "MxCube.Version")
    log_info "Migrating $board (current version: ${old_version:-unknown})..."

    # Create temporary script file for CubeMX
    local script_file
    script_file=$(mktemp)

    local ioc_for_cubemx
    local script_for_cubemx
    ioc_for_cubemx=$(posix_to_native_path "$ioc_file")
    script_for_cubemx=$(posix_to_native_path "$script_file")

    # Loading and saving with newer CubeMX auto-migrates the .ioc file
    cat > "$script_file" << EOF
config load "$ioc_for_cubemx"
config saveext "$ioc_for_cubemx"
exit
EOF

    # Run STM32CubeMX in script mode
    if "$CUBEMX_PATH" -q "$script_for_cubemx"; then
        local new_version
        new_version=$(ioc_get_value "$ioc_file" "MxCube.Version")
        log_info "Migration completed for $board (now version: ${new_version:-unknown})"
        rm -f "$script_file"
        return 0
    else
        log_error "Migration failed for $board"
        rm -f "$script_file"
        return 1
    fi
}

# Migrate all boards
migrate_all() {
    log_header "Migrating All Boards to Current CubeMX Version"

    local success=0
    local failed=0
    local skipped=0

    for board in "${BOARDS[@]}"; do
        if has_ioc_file "$board"; then
            echo -e "${CYAN}→${NC} Processing $board..."
            if migrate_ioc "$board"; then
                ((success++))
            else
                ((failed++))
            fi
        else
            log_warn "Skipping $board (no .ioc file)"
            ((skipped++))
        fi
    done

    echo ""
    log_header "Summary"
    echo -e "  ${GREEN}Succeeded:${NC} $success"
    echo -e "  ${RED}Failed:${NC}    $failed"
    echo -e "  ${YELLOW}Skipped:${NC}   $skipped"
}

# Update STM32 firmware packs
update_packs() {
    log_header "Updating STM32 Firmware Packs"

    log_info "Checking for firmware pack updates..."

    # Create temporary script file for CubeMX
    local script_file
    script_file=$(mktemp)

    cat > "$script_file" << EOF
swupdate refresh
swupdate install all
exit
EOF

    # Run STM32CubeMX in script mode
    if "$CUBEMX_PATH" -q "$script_file"; then
        log_info "Firmware pack update completed"
        rm -f "$script_file"
        return 0
    else
        log_error "Firmware pack update failed"
        rm -f "$script_file"
        return 1
    fi
}

# List all boards with .ioc status
list_boards() {
    log_header "Board IOC Status"

    printf "%-15s %-20s %-15s\n" "Board" "MCU" "Status"
    printf "%-15s %-20s %-15s\n" "-----" "---" "------"

    for board in "${BOARDS[@]}"; do
        local ioc_path
        ioc_path=$(get_ioc_path "$board")

        local status mcu
        if [ -f "$ioc_path" ]; then
            mcu=$(ioc_get_value "$ioc_path" "Mcu.Name")
            status="${GREEN}found${NC}"
        else
            mcu="-"
            status="${RED}missing${NC}"
        fi

        printf "%-15s %-20s %b\n" "$board" "$mcu" "$status"
    done
}

# Show board selection menu
show_board_menu() {
    echo -e "${BOLD}Select a board:${NC}"
    echo ""

    for i in "${!BOARDS[@]}"; do
        local board="${BOARDS[$i]}"
        local ioc_path
        ioc_path=$(get_ioc_path "$board")

        local status=""
        if [ -f "$ioc_path" ]; then
            local mcu
            mcu=$(ioc_get_value "$ioc_path" "Mcu.Name")
            status="${GREEN}[${mcu}]${NC}"
        else
            status="${RED}[no .ioc]${NC}"
        fi

        printf "  %d) %-15s %b\n" $((i + 1)) "$board" "$status"
    done

    echo ""
    echo "  q) Quit"
    echo ""
}

# Show operation menu for a board
show_operation_menu() {
    local board="$1"

    echo -e "${BOLD}Select operation for $board:${NC}"
    echo ""
    echo "  1) Generate code"
    echo "  2) Inspect configuration"
    echo "  3) Show pin assignments"
    echo "  4) Show peripherals"
    echo ""
    echo "  b) Back to board selection"
    echo "  q) Quit"
    echo ""
}

# Interactive board operations
interactive_board_operations() {
    local board="$1"

    while true; do
        show_operation_menu "$board"
        read -p "Select operation: " op

        case "$op" in
            1)
                if check_cubemx; then
                    generate_code "$board"
                else
                    show_cubemx_install_instructions
                fi
                ;;
            2) inspect_ioc "$board" ;;
            3) show_pins "$board" ;;
            4) show_peripherals "$board" ;;
            b|B) return 0 ;;
            q|Q) exit 0 ;;
            *) log_error "Invalid selection" ;;
        esac

        echo ""
        read -p "Press Enter to continue..."
    done
}

# Interactive mode
interactive_mode() {
    while true; do
        show_board_menu
        read -p "Enter selection: " selection

        case "$selection" in
            q|Q|quit|exit)
                return 0
                ;;
            [1-8])
                local index=$((selection - 1))
                if [ $index -ge 0 ] && [ $index -lt ${#BOARDS[@]} ]; then
                    local board="${BOARDS[$index]}"
                    if has_ioc_file "$board"; then
                        interactive_board_operations "$board"
                    else
                        log_error "No .ioc file for $board"
                    fi
                fi
                ;;
            *)
                # Check if it's a board name
                if is_valid_board "$selection"; then
                    if has_ioc_file "$selection"; then
                        interactive_board_operations "$selection"
                    else
                        log_error "No .ioc file for $selection"
                    fi
                else
                    log_error "Invalid selection: $selection"
                fi
                ;;
        esac
    done
}

# Show help
show_help() {
    cat << EOF
FEB Firmware STM32CubeMX IOC Management Script

Generate HAL code from .ioc files without the STM32CubeMX GUI.

Usage:
  ./scripts/cubemx.sh [options]

Options:
  -g, --generate         Generate HAL code from .ioc file
  -m, --migrate          Migrate .ioc file to current CubeMX version
  -i, --inspect          Display .ioc configuration summary
  -b, --board <BOARD>    Target specific board
  -a, --all              Process all boards
  --update-packs         Update STM32 firmware packs to latest versions
  --show-pins            Display pin assignments
  --show-peripherals     List enabled peripherals
  --list-boards          Show all boards with .ioc status
  -h, --help             Show this help message

Boards: ${BOARDS[*]} (or 'all')

Examples:
  ./scripts/cubemx.sh                    # Interactive menu
  ./scripts/cubemx.sh -g -b BMS          # Generate code for BMS
  ./scripts/cubemx.sh -m -b BMS          # Migrate BMS to current CubeMX version
  ./scripts/cubemx.sh -m -g -b BMS       # Migrate and generate for BMS
  ./scripts/cubemx.sh -a -m              # Migrate all boards
  ./scripts/cubemx.sh -a -g              # Generate code for all boards
  ./scripts/cubemx.sh -g -b all          # Generate code for all boards
  ./scripts/cubemx.sh --update-packs     # Update firmware packs
  ./scripts/cubemx.sh -i -b LVPDB        # Inspect LVPDB configuration
  ./scripts/cubemx.sh --show-pins -b PCU # Show PCU pin assignments
  ./scripts/cubemx.sh --list-boards      # List all boards

Prerequisites:
  - STM32CubeMX (for code generation, migration, and pack updates)

Note: Inspection commands work without STM32CubeMX by parsing .ioc files directly.
EOF
}

# Main function
main() {
    local board=""
    local operation=""
    local process_all=false
    local do_migrate=false
    local do_generate=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -g|--generate)
                do_generate=true
                shift
                ;;
            -m|--migrate)
                do_migrate=true
                shift
                ;;
            -i|--inspect)
                operation="inspect"
                shift
                ;;
            -b|--board)
                if [ -z "$2" ] || [[ "$2" == -* ]]; then
                    log_error "Option -b requires a board name"
                    exit 1
                fi
                board="$2"
                shift 2
                ;;
            -a|--all)
                process_all=true
                shift
                ;;
            --update-packs)
                operation="update-packs"
                shift
                ;;
            --show-pins)
                operation="pins"
                shift
                ;;
            --show-peripherals)
                operation="peripherals"
                shift
                ;;
            --list-boards)
                operation="list"
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

    # Convert flags to operation (for backwards compatibility)
    if [ "$do_migrate" = true ] && [ "$do_generate" = true ]; then
        operation="migrate-generate"
    elif [ "$do_migrate" = true ]; then
        operation="migrate"
    elif [ "$do_generate" = true ]; then
        operation="generate"
    fi

    log_header "FEB CubeMX Manager"

    # Handle list-boards (no CubeMX needed)
    if [ "$operation" = "list" ]; then
        list_boards
        exit 0
    fi

    # Handle update-packs (no board needed)
    if [ "$operation" = "update-packs" ]; then
        if ! check_cubemx; then
            show_cubemx_install_instructions
            exit 1
        fi
        update_packs
        exit 0
    fi

    # Validate board if specified
    if [ -n "$board" ] && [ "$board" != "all" ]; then
        if ! is_valid_board "$board"; then
            log_error "Invalid board: $board"
            echo "Valid boards: ${BOARDS[*]} all"
            exit 1
        fi
    fi

    # Handle batch operations
    if [ "$process_all" = true ] || [ "$board" = "all" ]; then
        if ! check_cubemx; then
            show_cubemx_install_instructions
            exit 1
        fi
        case "$operation" in
            generate)
                generate_all
                ;;
            migrate)
                migrate_all
                ;;
            migrate-generate)
                migrate_all
                generate_all
                ;;
            *)
                log_error "Specify an operation: -g (generate), -m (migrate), or both"
                exit 1
                ;;
        esac
        exit 0
    fi

    # Handle single board operations
    if [ -n "$board" ]; then
        case "$operation" in
            generate)
                if ! check_cubemx; then
                    show_cubemx_install_instructions
                    exit 1
                fi
                generate_code "$board"
                ;;
            migrate)
                if ! check_cubemx; then
                    show_cubemx_install_instructions
                    exit 1
                fi
                migrate_ioc "$board"
                ;;
            migrate-generate)
                if ! check_cubemx; then
                    show_cubemx_install_instructions
                    exit 1
                fi
                migrate_ioc "$board"
                generate_code "$board"
                ;;
            inspect)
                inspect_ioc "$board"
                ;;
            pins)
                show_pins "$board"
                ;;
            peripherals)
                show_peripherals "$board"
                ;;
            *)
                # Default to inspect if no operation specified
                inspect_ioc "$board"
                ;;
        esac
        exit 0
    fi

    # Default to interactive mode
    interactive_mode
}

main "$@"
