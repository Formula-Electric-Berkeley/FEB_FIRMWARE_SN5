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

    # Check ARM GCC
    if ! check_command "arm-none-eabi-gcc" "ARM GCC Toolchain" ""; then
        missing=true
        show_arm_gcc_install_hint
    fi

    # Check CMake
    if ! check_command "cmake" "CMake" ""; then
        missing=true
        show_cmake_install_hint
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

    # Check Ninja
    if ! check_command "ninja" "Ninja Build" ""; then
        missing=true
        show_ninja_install_hint
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
        log_error "Missing prerequisites. Install the required tools and try again."
        return 1
    fi

    log_info "All prerequisites satisfied"
    return 0
}

show_arm_gcc_install_hint() {
    echo ""
    case "$(uname -s)" in
        Darwin*)
            echo -e "    ${CYAN}macOS:${NC}"
            echo "      brew install --cask gcc-arm-embedded"
            echo "      OR download from: https://developer.arm.com/downloads/-/gnu-rm"
            ;;
        Linux*)
            echo -e "    ${CYAN}Linux:${NC}"
            echo "      sudo apt install gcc-arm-none-eabi"
            echo "      OR download from: https://developer.arm.com/downloads/-/gnu-rm"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo -e "    ${CYAN}Windows:${NC}"
            echo "      Download from: https://developer.arm.com/downloads/-/gnu-rm"
            echo "      Add bin/ directory to PATH"
            ;;
    esac
}

show_cmake_install_hint() {
    echo ""
    case "$(uname -s)" in
        Darwin*)
            echo -e "    ${CYAN}macOS:${NC} brew install cmake"
            ;;
        Linux*)
            echo -e "    ${CYAN}Linux:${NC} sudo apt install cmake"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo -e "    ${CYAN}Windows:${NC} Download from https://cmake.org/download/"
            ;;
    esac
}

show_ninja_install_hint() {
    echo ""
    case "$(uname -s)" in
        Darwin*)
            echo -e "    ${CYAN}macOS:${NC} brew install ninja"
            ;;
        Linux*)
            echo -e "    ${CYAN}Linux:${NC} sudo apt install ninja-build"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo -e "    ${CYAN}Windows:${NC} Download from https://ninja-build.org/"
            ;;
    esac
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

# Get build directory based on build type
get_build_dir() {
    local build_type="$1"
    echo "$REPO_ROOT/build/$build_type"
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
    local build_dir
    build_dir=$(get_build_dir "$build_type")

    echo -e "${BOLD}Select board(s) to build:${NC}"
    echo ""

    for i in "${!BOARDS[@]}"; do
        local board="${BOARDS[$i]}"
        local elf_path="$build_dir/$board/$board.elf"

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
    echo "  a) Build all boards"
    echo "  q) Quit"
    echo ""
}

# Interactive board selection
interactive_select() {
    local build_type="$1"
    local clean="$2"

    show_board_menu "$build_type"

    read -p "Enter selection (1-7, a, or q): " selection

    case "$selection" in
        q|Q|quit|exit)
            return 1
            ;;
        a|A|all)
            if [ "$clean" = true ]; then
                clean_build "$build_type"
            fi
            configure_cmake "$build_type"
            build_targets "$build_type"
            show_summary "$build_type"
            return 0
            ;;
        [1-7])
            local index=$((selection - 1))
            if [ $index -ge 0 ] && [ $index -lt ${#BOARDS[@]} ]; then
                local board="${BOARDS[$index]}"
                if [ "$clean" = true ]; then
                    clean_build "$build_type"
                fi
                configure_cmake "$build_type"
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
                configure_cmake "$build_type"
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

# Show build summary
show_summary() {
    local build_type="$1"
    shift
    local targets=("$@")
    local build_dir
    build_dir=$(get_build_dir "$build_type")

    log_header "Build Summary"

    local boards_to_check=("${targets[@]}")
    if [ ${#boards_to_check[@]} -eq 0 ]; then
        boards_to_check=("${BOARDS[@]}")
    fi

    printf "%-15s %-10s %-12s %-12s\n" "Board" "Status" "Flash" "RAM"
    printf "%-15s %-10s %-12s %-12s\n" "-----" "------" "-----" "---"

    for board in "${boards_to_check[@]}"; do
        local elf_path="$build_dir/$board/$board.elf"

        if [ -f "$elf_path" ]; then
            # Get size info
            local size_output
            size_output=$(arm-none-eabi-size "$elf_path" 2>/dev/null | tail -1)
            local text data bss
            text=$(echo "$size_output" | awk '{print $1}')
            data=$(echo "$size_output" | awk '{print $2}')
            bss=$(echo "$size_output" | awk '{print $3}')

            local flash_kb data_kb
            flash_kb=$(echo "scale=1; ($text + $data) / 1024" | bc)
            ram_kb=$(echo "scale=1; ($data + $bss) / 1024" | bc)

            printf "%-15s ${GREEN}%-10s${NC} %-12s %-12s\n" "$board" "OK" "${flash_kb} KB" "${ram_kb} KB"
        else
            printf "%-15s ${YELLOW}%-10s${NC} %-12s %-12s\n" "$board" "skipped" "-" "-"
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

Examples:
  ./scripts/build.sh                     # Interactive menu
  ./scripts/build.sh -a                  # Build all boards (Debug)
  ./scripts/build.sh -b LVPDB            # Build LVPDB only
  ./scripts/build.sh -b LVPDB -b PCU     # Build multiple boards
  ./scripts/build.sh -r -a               # Build all in Release mode
  ./scripts/build.sh -c -b LVPDB         # Clean rebuild of LVPDB
  ./scripts/build.sh -l                  # Loop mode (build multiple)
  ./scripts/build.sh --check             # Verify toolchain is installed

After building, flash with:
  ./scripts/flash.sh -b LVPDB
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

        configure_cmake "$build_type"

        if [ "$build_all" = true ]; then
            build_targets "$build_type"
            show_summary "$build_type"
        else
            build_targets "$build_type" "${targets[@]}"
            show_summary "$build_type" "${targets[@]}"
        fi
    fi

    log_info "Build complete!"
}

main "$@"
