#!/bin/bash
#
# FEB Firmware Build Script
#
# Validates prerequisites, configures CMake if needed, and builds firmware.
#
# Usage:
#   ./scripts/build.sh                 # Build all boards (Debug)
#   ./scripts/build.sh LVPDB           # Build specific board
#   ./scripts/build.sh LVPDB PCU       # Build multiple boards
#   ./scripts/build.sh --release       # Build in Release mode
#   ./scripts/build.sh --clean         # Clean and rebuild
#   ./scripts/build.sh -h              # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

source "$SCRIPT_DIR/lib/cube-tools.sh"

# Available boards. Sensor_Nodes ships in two CMake targets — Sensor_Nodes_FRONT
# and Sensor_Nodes_REAR — each a separately-buildable executable defined in
# Sensor_Nodes/CMakeLists.txt. Both build into the shared build/Debug tree.
# See Sensor_Nodes/Core/User/Inc/FEB_SN_Config.h for variant configuration.
source "$SCRIPT_DIR/lib/boards.sh"
BOARDS=("${FEB_BOARD_TARGETS[@]}")

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

# Check if a command exists
check_command() {
    local cmd="$1"
    local name="$2"
    local install_hint="$3"

    if command -v "$cmd" &> /dev/null; then
        local version
        version=$("$cmd" --version 2>&1 | head -1)
        echo -e "  ${GREEN}✓${NC} $name: $version"
        return 0
    else
        echo -e "  ${RED}✗${NC} $name: not found"
        if [ -n "$install_hint" ]; then
            echo -e "    ${CYAN}→${NC} $install_hint"
        fi
        return 1
    fi
}

# Check all prerequisites
check_prerequisites() {
    log_header "Checking Prerequisites"

    local missing=false

    if ! feb_use_cube_tools "$REPO_ROOT" cmake; then
        if [ -n "$FEB_CUBE" ]; then
            log_warn "cube could not resolve the pinned CMake bundle. run: cube bundle install --project"
        else
            log_warn "cube not found. falling back to tools on PATH"
        fi
    fi
    echo -e "  Toolchain source: ${BOLD}${FEB_TOOLS_SOURCE}${NC}"

    if ! check_command "arm-none-eabi-gcc" "ARM GCC Toolchain" ""; then
        missing=true
    fi

    if ! check_command "cmake" "CMake" ""; then
        missing=true
    else
        # Check CMake version >= 3.22
        local cmake_version
        cmake_version=$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
        local major minor
        major=$(echo "$cmake_version" | cut -d. -f1)
        minor=$(echo "$cmake_version" | cut -d. -f2)
        if [ "$major" -lt 3 ] || { [ "$major" -eq 3 ] && [ "$minor" -lt 22 ]; }; then
            echo -e "    ${YELLOW}⚠${NC}  CMake 3.22+ required (found $cmake_version)"
            missing=true
        fi
    fi

    if ! check_command "ninja" "Ninja Build" ""; then
        missing=true
    fi

    # Optional: Check clang-format
    if command -v clang-format &> /dev/null; then
        local cf_version
        cf_version=$(clang-format --version 2>&1 | head -1)
        echo -e "  ${GREEN}✓${NC} clang-format: $cf_version"
    else
        echo -e "  ${YELLOW}○${NC} clang-format: not found (optional, for formatting)"
    fi

    echo ""

    if [ "$missing" = true ]; then
        log_error "Missing prerequisites."
        feb_print_cube_install_hint
        return 1
    fi

    log_info "All prerequisites satisfied"
    return 0
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

# Get build directory based on build type. All boards (including
# Sensor_Nodes_FRONT and Sensor_Nodes_REAR) share the same tree; variants
# are real CMake targets, not configure-time switches.
get_build_dir() {
    local build_type="$1"
    echo "$REPO_ROOT/build/$build_type"
}

# Get the path of the produced ELF for a board (= CMake target name).
get_target_elf() {
    local build_type="$1"
    local board="$2"
    local build_dir
    build_dir=$(get_build_dir "$build_type")
    case "$board" in
        Sensor_Nodes_FRONT|Sensor_Nodes_REAR)
            # Variant targets live under the Sensor_Nodes subdir.
            echo "$build_dir/Sensor_Nodes/${board}.elf"
            ;;
        *)
            echo "$build_dir/$board/$board.elf"
            ;;
    esac
}

# Configure CMake if needed
configure_cmake() {
    local build_type="$1"
    local build_dir
    build_dir=$(get_build_dir "$build_type")

    if [ ! -f "$build_dir/build.ninja" ]; then
        log_info "Configuring CMake ($build_type)..."
        cmake --preset "$build_type" -S "$REPO_ROOT"
    fi
}

# Build target(s)
build_targets() {
    local build_type="$1"
    shift
    local targets=("$@")
    local build_dir
    build_dir=$(get_build_dir "$build_type")

    configure_cmake "$build_type"

    if [ ${#targets[@]} -eq 0 ]; then
        log_info "Building all boards ($build_type)..."
        cmake --build "$build_dir"
    else
        for target in "${targets[@]}"; do
            log_info "Building $target ($build_type)..."
            cmake --build "$build_dir" --target "$target"
        done
    fi
}

# Clean build directory
clean_build() {
    local build_type="$1"
    local build_dir
    build_dir=$(get_build_dir "$build_type")

    if [ -d "$build_dir" ]; then
        log_info "Cleaning $build_dir..."
        rm -rf "$build_dir"
    fi
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
    local build_type="$1"

    echo -e "${BOLD}Select board(s) to build:${NC}"
    echo ""

    for i in "${!BOARDS[@]}"; do
        local board="${BOARDS[$i]}"
        local elf_path
        elf_path=$(get_target_elf "$build_type" "$board")

        local status=""
        if [ -f "$elf_path" ]; then
            local build_time
            build_time=$(get_file_time "$elf_path")
            status="${GREEN}[built ${build_time}]${NC}"
        else
            status="${YELLOW}[not built]${NC}"
        fi

        printf "  %2d) %-22s %b\n" $((i + 1)) "$board" "$status"
    done

    echo ""
    echo "   a) Build all boards"
    echo "   q) Quit"
    echo ""
}

# Interactive board selection
interactive_select() {
    local build_type="$1"
    local clean="$2"

    show_board_menu "$build_type"

    read -p "Enter selection (number, board name, a, or q): " selection

    case "$selection" in
        q|Q|quit|exit)
            return 1
            ;;
        a|A|all)
            if [ "$clean" = true ]; then
                clean_build "$build_type"
            fi
            build_targets "$build_type" "${BOARDS[@]}"
            show_summary "$build_type" "${BOARDS[@]}"
            return 0
            ;;
        [0-9]|[0-9][0-9])
            local index=$((selection - 1))
            if [ $index -ge 0 ] && [ $index -lt ${#BOARDS[@]} ]; then
                local board="${BOARDS[$index]}"
                if [ "$clean" = true ]; then
                    clean_build "$build_type"
                fi
                build_targets "$build_type" "$board"
                show_summary "$build_type" "$board"
                return 0
            fi
            ;;
        *)
            # Check if it's a board name
            if is_valid_board "$selection"; then
                if [ "$clean" = true ]; then
                    clean_build "$build_type"
                fi
                build_targets "$build_type" "$selection"
                show_summary "$build_type" "$selection"
                return 0
            fi

            log_error "Invalid selection: $selection"
            return 1
            ;;
    esac
}

# Interactive loop mode
interactive_loop() {
    local build_type="$1"
    local clean="$2"

    log_header "Interactive Build Mode"
    echo "Build boards interactively. Enter 'q' to quit."
    echo ""

    while true; do
        if ! interactive_select "$build_type" "$clean"; then
            break
        fi
        echo ""
        # Only clean on first iteration
        clean=false
    done

    log_info "Exiting interactive mode."
}

# Check that every submodule is checked out at the commit this branch pins.
# A stale common/FEB_CAN_Library_SN4 builds against an old DBC, so boards
# disagree on frame layouts and state numbers (a DASH built that way showed a
# BMS precharge fault as BOOT). Set FEB_ALLOW_SUBMODULE_DRIFT=1 to downgrade
# this to a warning while iterating on a submodule before bumping its pin.
check_submodules() {
    local status
    status=$(git -C "$REPO_ROOT" submodule status 2>/dev/null) || return 0

    local bad=false line path
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        path=$(echo "$line" | awk '{print $2}')
        case "${line:0:1}" in
            -) log_error "Submodule $path is not initialized"; bad=true ;;
            +) log_error "Submodule $path is checked out at a different commit than this branch pins"; bad=true ;;
            U) log_error "Submodule $path has merge conflicts"; bad=true ;;
        esac
    done <<< "$status"

    if [ "$bad" = false ]; then
        log_info "Submodules match their pinned commits"
        return 0
    fi

    echo -e "    ${CYAN}→${NC} Run: git submodule update --init --recursive"
    if [ "${FEB_ALLOW_SUBMODULE_DRIFT:-0}" = "1" ]; then
        log_warn "FEB_ALLOW_SUBMODULE_DRIFT=1: building anyway; do not flash this to the car"
        return 0
    fi
    return 1
}

# Show build summary
show_summary() {
    local build_type="$1"
    shift
    local targets=("$@")

    log_header "Build Summary"

    local boards_to_check=("${targets[@]}")
    if [ ${#boards_to_check[@]} -eq 0 ]; then
        boards_to_check=("${BOARDS[@]}")
    fi

    printf "%-22s %-10s %-12s %-12s\n" "Board" "Status" "Flash" "RAM"
    printf "%-22s %-10s %-12s %-12s\n" "-----" "------" "-----" "---"

    for board in "${boards_to_check[@]}"; do
        local elf_path
        elf_path=$(get_target_elf "$build_type" "$board")

        if [ -f "$elf_path" ]; then
            # Get size info
            local size_output
            size_output=$(arm-none-eabi-size "$elf_path" 2>/dev/null | tail -1)
            local text data bss
            text=$(echo "$size_output" | awk '{print $1}')
            data=$(echo "$size_output" | awk '{print $2}')
            bss=$(echo "$size_output" | awk '{print $3}')

            local flash_kb ram_kb
            flash_kb=$(awk -v n="$((text + data))" 'BEGIN { printf "%.1f", n / 1024 }')
            ram_kb=$(awk -v n="$((data + bss))" 'BEGIN { printf "%.1f", n / 1024 }')

            printf "%-22s ${GREEN}%-10s${NC} %-12s %-12s\n" "$board" "OK" "${flash_kb} KB" "${ram_kb} KB"
        else
            printf "%-22s ${YELLOW}%-10s${NC} %-12s %-12s\n" "$board" "skipped" "-" "-"
        fi
    done

    echo ""
}

# Show help
show_help() {
    cat << EOF
FEB Firmware Build Script

Validates prerequisites, configures CMake if needed, and builds firmware.

Usage:
  ./scripts/build.sh [options] [boards...]

Options:
  -i, --interactive   Interactive mode: select boards from a menu
  -l, --loop          Loop mode: build multiple boards interactively
  -a, --all           Build all boards
  -b, --board BOARD   Build specific board (can be repeated)
  -r, --release       Build in Release mode (optimized, no debug symbols)
  -c, --clean         Clean build directory before building
  --check             Only check prerequisites, don't build
  -h, --help          Show this help message

Boards: ${BOARDS[*]}

Note:
  Sensor_Nodes_FRONT and Sensor_Nodes_REAR are real CMake targets — both build
  into build/Debug/Sensor_Nodes/ as Sensor_Nodes_FRONT.elf and Sensor_Nodes_REAR.elf
  in a single configure. They're also visible in the VS Code CMake Tools
  build-target dropdown.

Examples:
  ./scripts/build.sh                          # Interactive menu
  ./scripts/build.sh -a                       # Build all boards (Debug)
  ./scripts/build.sh -b LVPDB                 # Build LVPDB only
  ./scripts/build.sh -b Sensor_Nodes_FRONT    # Build the FRONT sensor node
  ./scripts/build.sh -b Sensor_Nodes_REAR     # Build the REAR sensor node
  ./scripts/build.sh -b LVPDB -b PCU          # Build multiple boards
  ./scripts/build.sh -r -a                    # Build all in Release mode
  ./scripts/build.sh -c -b LVPDB              # Clean rebuild of LVPDB
  ./scripts/build.sh -l                       # Loop mode (build multiple)
  ./scripts/build.sh --check                  # Verify toolchain is installed

Environment:
  FEB_ALLOW_SUBMODULE_DRIFT=1   Warn instead of failing when a submodule (e.g. the
                                CAN library) isn't at the commit this branch pins

After building, flash with:
  ./scripts/flash.sh -b LVPDB
  ./scripts/flash.sh -b Sensor_Nodes_FRONT
EOF
}

# Main function
main() {
    local build_type="Debug"
    local clean=false
    local check_only=false
    local interactive=false
    local loop=false
    local build_all=false
    local targets=()

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -i|--interactive)
                interactive=true
                shift
                ;;
            -l|--loop)
                loop=true
                shift
                ;;
            -a|--all)
                build_all=true
                shift
                ;;
            -b|--board)
                if [ -z "$2" ] || [[ "$2" == -* ]]; then
                    log_error "Option -b requires a board name"
                    exit 1
                fi
                if is_valid_board "$2"; then
                    targets+=("$2")
                else
                    log_error "Unknown board: $2"
                    echo "Valid boards: ${BOARDS[*]}"
                    exit 1
                fi
                shift 2
                ;;
            -r|--release)
                build_type="Release"
                shift
                ;;
            -c|--clean)
                clean=true
                shift
                ;;
            --check)
                check_only=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            -*)
                log_error "Unknown option: $1"
                echo ""
                show_help
                exit 1
                ;;
            *)
                # Allow board names as positional args for backwards compat
                if is_valid_board "$1"; then
                    targets+=("$1")
                else
                    log_error "Unknown board: $1"
                    echo "Valid boards: ${BOARDS[*]}"
                    exit 1
                fi
                shift
                ;;
        esac
    done

    log_header "FEB Firmware Build"

    # Check prerequisites
    if ! check_prerequisites; then
        exit 1
    fi

    if ! check_submodules; then
        exit 1
    fi

    # Exit early if only checking
    if [ "$check_only" = true ]; then
        exit 0
    fi

    cd "$REPO_ROOT"

    # Determine build mode
    if [ "$loop" = true ]; then
        # Loop mode - keep building interactively
        interactive_loop "$build_type" "$clean"
    elif [ "$interactive" = true ] || { [ ${#targets[@]} -eq 0 ] && [ "$build_all" = false ]; }; then
        # Interactive mode - single selection (default when no targets specified)
        interactive_select "$build_type" "$clean"
    else
        # Direct build mode
        if [ "$clean" = true ]; then
            clean_build "$build_type"
        fi

        if [ "$build_all" = true ]; then
            build_targets "$build_type" "${BOARDS[@]}"
            show_summary "$build_type" "${BOARDS[@]}"
        else
            build_targets "$build_type" "${targets[@]}"
            show_summary "$build_type" "${targets[@]}"
        fi
    fi

    log_info "Build complete!"
}

main "$@"
