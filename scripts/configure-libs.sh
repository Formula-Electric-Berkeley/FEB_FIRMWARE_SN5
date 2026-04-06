#!/bin/bash
#
# FEB Firmware Library Configuration Script
#
# Configures CMakeLists.txt to integrate common FEB libraries.
#
# Usage:
#   ./scripts/configure-libs.sh -b BOARD [options]
#   ./scripts/configure-libs.sh --list
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."

# Available boards (from root CMakeLists.txt)
BOARDS=("BMS" "DASH" "DART" "DCU" "LVPDB" "PCU" "Sensor_Nodes" "UART" "UART_TEST")

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

# ══════════════════════════════════════════════════════════════════════════════
#  DETECTION FUNCTIONS
# ══════════════════════════════════════════════════════════════════════════════

# Check if board has FreeRTOS middleware
has_freertos() {
    local board="$1"
    [ -d "$REPO_ROOT/$board/Middlewares/Third_Party/FreeRTOS" ]
}

# Check if FEB_UART_USE_FREERTOS is defined in CMakeLists
has_freertos_define() {
    local cmake_file="$1"
    grep -q "FEB_UART_USE_FREERTOS" "$cmake_file" 2>/dev/null
}

# Check if CAN HAL is enabled in the board configuration
has_can_hal() {
    local board="$1"
    local hal_conf="$REPO_ROOT/$board/Core/Inc/stm32f4xx_hal_conf.h"
    if [ -f "$hal_conf" ]; then
        # Check if HAL_CAN_MODULE_ENABLED is defined and not commented out
        grep -q "^#define HAL_CAN_MODULE_ENABLED" "$hal_conf" 2>/dev/null
    else
        return 1
    fi
}

# Check if I2C HAL is enabled in the board configuration (required for TPS)
has_i2c_hal() {
    local board="$1"
    local hal_conf="$REPO_ROOT/$board/Core/Inc/stm32f4xx_hal_conf.h"
    if [ -f "$hal_conf" ]; then
        grep -q "^#define HAL_I2C_MODULE_ENABLED" "$hal_conf" 2>/dev/null
    else
        return 1
    fi
}

# Check if UART HAL is enabled in the board configuration (required for feb_io)
has_uart_hal() {
    local board="$1"
    local hal_conf="$REPO_ROOT/$board/Core/Inc/stm32f4xx_hal_conf.h"
    if [ -f "$hal_conf" ]; then
        grep -q "^#define HAL_UART_MODULE_ENABLED" "$hal_conf" 2>/dev/null
    else
        return 1
    fi
}

# Check if CAN library sources are integrated
has_can_sources() {
    local cmake_file="$1"
    grep -q "FEB_CAN_Library" "$cmake_file" 2>/dev/null
}

# Check if FreeRTOS sources are integrated
has_freertos_sources() {
    local cmake_file="$1"
    grep -q "FREERTOS_CORE" "$cmake_file" 2>/dev/null
}

# Check if a library is linked
has_lib() {
    local cmake_file="$1"
    local lib="$2"
    grep -E "target_link_libraries.*$lib" "$cmake_file" 2>/dev/null | grep -qv "^#"
}

# Get list of currently linked libraries
get_current_libs() {
    local cmake_file="$1"
    grep -E "^target_link_libraries\(" "$cmake_file" 2>/dev/null | \
        sed -E 's/.*PRIVATE ([^)]+)\)/\1/' | \
        tr ' ' '\n' | \
        grep -v '^m$' | \
        sort -u
}

# ══════════════════════════════════════════════════════════════════════════════
#  MODIFICATION FUNCTIONS
# ══════════════════════════════════════════════════════════════════════════════

# Detect sed in-place flag (macOS vs GNU)
get_sed_inplace() {
    if [[ "$(uname -s)" == "Darwin" ]]; then
        echo "-i ''"
    else
        echo "-i"
    fi
}

# Add FreeRTOS compile definition
add_freertos_define() {
    local cmake_file="$1"
    local dry_run="$2"

    if has_freertos_define "$cmake_file"; then
        log_info "FEB_UART_USE_FREERTOS already defined"
        return 0
    fi

    if [ "$dry_run" = true ]; then
        echo -e "${CYAN}+ Would add:${NC} FEB_UART_USE_FREERTOS=1 compile definition"
        return 0
    fi

    log_info "Adding FEB_UART_USE_FREERTOS=1 compile definition..."

    # Insert before $<$<CONFIG:Debug>:DEBUG>
    if [[ "$(uname -s)" == "Darwin" ]]; then
        sed -i '' 's/\(\$<\$<CONFIG:Debug>:DEBUG>\)/FEB_UART_USE_FREERTOS=1\
    \1/' "$cmake_file"
    else
        sed -i 's/\(\$<\$<CONFIG:Debug>:DEBUG>\)/FEB_UART_USE_FREERTOS=1\n    \1/' "$cmake_file"
    fi
}

# Add FreeRTOS sources collection
add_freertos_sources() {
    local cmake_file="$1"
    local dry_run="$2"

    if has_freertos_sources "$cmake_file"; then
        log_info "FreeRTOS sources already configured"
        return 0
    fi

    if [ "$dry_run" = true ]; then
        echo -e "${CYAN}+ Would add:${NC} FreeRTOS source collection (FREERTOS_CORE, FREERTOS_CMSIS, etc.)"
        return 0
    fi

    log_info "Adding FreeRTOS source collection..."

    # Find line number of "set(ALL_SOURCES" and insert after it
    local line_num
    line_num=$(grep -n "^set(ALL_SOURCES" "$cmake_file" | head -1 | cut -d: -f1)

    if [ -z "$line_num" ]; then
        log_error "Could not find 'set(ALL_SOURCES' in CMakeLists.txt"
        return 1
    fi

    # Create temp file with insertion
    local temp_file
    temp_file=$(mktemp)

    head -n "$line_num" "$cmake_file" > "$temp_file"
    cat >> "$temp_file" << 'FREERTOS_BLOCK'

# ── Middleware: FreeRTOS ─────────────────────────────────────────────
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS")
    file(GLOB FREERTOS_CORE "Middlewares/Third_Party/FreeRTOS/Source/*.c")
    file(GLOB FREERTOS_CMSIS "Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/*.c")
    file(GLOB FREERTOS_PORT "Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/*.c")
    file(GLOB FREERTOS_HEAP "Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c")
    list(APPEND ALL_SOURCES ${FREERTOS_CORE} ${FREERTOS_CMSIS} ${FREERTOS_PORT} ${FREERTOS_HEAP})
endif()
FREERTOS_BLOCK
    tail -n +"$((line_num + 1))" "$cmake_file" >> "$temp_file"
    mv "$temp_file" "$cmake_file"
}

# Add CAN library sources
add_can_sources() {
    local cmake_file="$1"
    local dry_run="$2"

    if has_can_sources "$cmake_file"; then
        log_info "CAN library sources already configured"
        return 0
    fi

    if [ "$dry_run" = true ]; then
        echo -e "${CYAN}+ Would add:${NC} CAN library source collection (FEB_CAN_Library_SN4/gen + FEB_CAN_Library/Src)"
        return 0
    fi

    log_info "Adding CAN library sources..."

    # Find the best insertion point - after FreeRTOS block or after set(ALL_SOURCES)
    local insert_after
    if grep -q "^endif().*# FreeRTOS" "$cmake_file" 2>/dev/null || grep -q "FREERTOS_HEAP" "$cmake_file" 2>/dev/null; then
        # After FreeRTOS endif
        insert_after=$(grep -n "list(APPEND ALL_SOURCES \${FREERTOS" "$cmake_file" | tail -1 | cut -d: -f1)
        if [ -z "$insert_after" ]; then
            insert_after=$(grep -n "endif()" "$cmake_file" | head -1 | cut -d: -f1)
        fi
    else
        # After set(ALL_SOURCES)
        insert_after=$(grep -n "^set(ALL_SOURCES" "$cmake_file" | head -1 | cut -d: -f1)
    fi

    if [ -z "$insert_after" ]; then
        log_error "Could not find insertion point for CAN sources"
        return 1
    fi

    local temp_file
    temp_file=$(mktemp)

    head -n "$insert_after" "$cmake_file" > "$temp_file"
    cat >> "$temp_file" << 'CAN_BLOCK'

# ── FEB CAN Library sources ──────────────────────────────────────────
file(GLOB CAN_LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library_SN4/gen/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library/Src/*.c"
)
list(APPEND ALL_SOURCES ${CAN_LIB_SOURCES})
CAN_BLOCK
    tail -n +"$((insert_after + 1))" "$cmake_file" >> "$temp_file"
    mv "$temp_file" "$cmake_file"
}

# Add FreeRTOS includes
add_freertos_includes() {
    local cmake_file="$1"
    local dry_run="$2"

    if grep -q "FreeRTOS/Source/include" "$cmake_file" 2>/dev/null; then
        log_info "FreeRTOS includes already configured"
        return 0
    fi

    if [ "$dry_run" = true ]; then
        echo -e "${CYAN}+ Would add:${NC} FreeRTOS include directories"
        return 0
    fi

    log_info "Adding FreeRTOS include directories..."

    # Find line after BASE_INCLUDES closing paren
    local line_num
    line_num=$(grep -n "^)" "$cmake_file" | head -1 | cut -d: -f1)

    # Find the closing paren of set(BASE_INCLUDES
    local in_base_includes=false
    local paren_line=0
    while IFS= read -r line; do
        paren_line=$((paren_line + 1))
        if echo "$line" | grep -q "^set(BASE_INCLUDES"; then
            in_base_includes=true
        fi
        if [ "$in_base_includes" = true ] && echo "$line" | grep -q "^)"; then
            break
        fi
    done < "$cmake_file"

    if [ "$paren_line" -eq 0 ]; then
        log_error "Could not find BASE_INCLUDES closing paren"
        return 1
    fi

    local temp_file
    temp_file=$(mktemp)

    head -n "$paren_line" "$cmake_file" > "$temp_file"
    cat >> "$temp_file" << 'FREERTOS_INC_BLOCK'

# FreeRTOS includes
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS")
    list(APPEND BASE_INCLUDES
        Middlewares/Third_Party/FreeRTOS/Source/include
        Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
        Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
    )
endif()
FREERTOS_INC_BLOCK
    tail -n +"$((paren_line + 1))" "$cmake_file" >> "$temp_file"
    mv "$temp_file" "$cmake_file"
}

# Add CAN library includes
add_can_includes() {
    local cmake_file="$1"
    local dry_run="$2"

    if grep -q 'CAN_LIB_DIR.*common/FEB_CAN_Library"' "$cmake_file" 2>/dev/null; then
        log_info "CAN library includes already configured"
        return 0
    fi

    if [ "$dry_run" = true ]; then
        echo -e "${CYAN}+ Would add:${NC} CAN library include directories"
        return 0
    fi

    log_info "Adding CAN library include directories..."

    # Find the auto-discover Core/User/Inc line and insert before it
    local line_num
    line_num=$(grep -n "Auto-discover Core/User/Inc" "$cmake_file" | head -1 | cut -d: -f1)

    if [ -z "$line_num" ]; then
        # Fallback: insert after FreeRTOS includes or after BASE_INCLUDES
        if grep -q "FreeRTOS/Source/include" "$cmake_file"; then
            line_num=$(grep -n "endif().*#.*FreeRTOS" "$cmake_file" | head -1 | cut -d: -f1)
            if [ -z "$line_num" ]; then
                # Find endif after FreeRTOS includes
                local freertos_line
                freertos_line=$(grep -n "FreeRTOS/Source/include" "$cmake_file" | head -1 | cut -d: -f1)
                line_num=$(tail -n +"$freertos_line" "$cmake_file" | grep -n "^endif()" | head -1 | cut -d: -f1)
                line_num=$((freertos_line + line_num - 1))
            fi
        else
            # After BASE_INCLUDES closing paren
            local in_base_includes=false
            local paren_line=0
            while IFS= read -r line; do
                paren_line=$((paren_line + 1))
                if echo "$line" | grep -q "^set(BASE_INCLUDES"; then
                    in_base_includes=true
                fi
                if [ "$in_base_includes" = true ] && echo "$line" | grep -q "^)"; then
                    break
                fi
            done < "$cmake_file"
            line_num=$paren_line
        fi
    else
        # Insert before the comment line
        line_num=$((line_num - 1))
    fi

    if [ -z "$line_num" ] || [ "$line_num" -eq 0 ]; then
        log_error "Could not find insertion point for CAN includes"
        return 1
    fi

    local temp_file
    temp_file=$(mktemp)

    head -n "$line_num" "$cmake_file" > "$temp_file"
    cat >> "$temp_file" << 'CAN_INC_BLOCK'

# FEB CAN Library includes
set(CAN_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library")
list(APPEND BASE_INCLUDES
    "${CAN_LIB_DIR}/Inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/FEB_CAN_Library_SN4/gen"
)

CAN_INC_BLOCK
    tail -n +"$((line_num + 1))" "$cmake_file" >> "$temp_file"
    mv "$temp_file" "$cmake_file"
}

# Update target_link_libraries
update_link_libraries() {
    local cmake_file="$1"
    local dry_run="$2"
    local add_io="$3"
    local add_can="$4"
    local add_tps="$5"
    local add_rtos_utils="$6"

    # Build list of libraries to add
    local libs_to_add=""
    [ "$add_io" = true ] && libs_to_add="$libs_to_add feb_io"
    [ "$add_can" = true ] && libs_to_add="$libs_to_add feb_can"
    [ "$add_tps" = true ] && libs_to_add="$libs_to_add feb_tps"
    [ "$add_rtos_utils" = true ] && libs_to_add="$libs_to_add feb_rtos_utils"

    # Remove libs that are already present
    local current_libs
    current_libs=$(get_current_libs "$cmake_file")

    local new_libs=""
    for lib in $libs_to_add; do
        if ! echo "$current_libs" | grep -q "^${lib}$"; then
            new_libs="$new_libs $lib"
        fi
    done

    if [ -z "$new_libs" ]; then
        log_info "All requested libraries already linked"
        return 0
    fi

    if [ "$dry_run" = true ]; then
        echo -e "${CYAN}+ Would add to target_link_libraries:${NC}$new_libs"
        return 0
    fi

    log_info "Adding libraries to target_link_libraries:$new_libs"

    # Update the target_link_libraries line
    if [[ "$(uname -s)" == "Darwin" ]]; then
        sed -i '' "s/target_link_libraries(\${PROJECT_NAME} PRIVATE m)/target_link_libraries(\${PROJECT_NAME} PRIVATE m$new_libs)/" "$cmake_file"
    else
        sed -i "s/target_link_libraries(\${PROJECT_NAME} PRIVATE m)/target_link_libraries(\${PROJECT_NAME} PRIVATE m$new_libs)/" "$cmake_file"
    fi

    # If that didn't work (libs already present), try appending
    if ! grep -q "$new_libs" "$cmake_file"; then
        # More complex replacement needed - find existing libs and append
        local current_line
        current_line=$(grep "target_link_libraries.*PRIVATE" "$cmake_file")
        local new_line
        new_line=$(echo "$current_line" | sed "s/)/$new_libs)/")
        if [[ "$(uname -s)" == "Darwin" ]]; then
            sed -i '' "s|$current_line|$new_line|" "$cmake_file"
        else
            sed -i "s|$current_line|$new_line|" "$cmake_file"
        fi
    fi
}

# ══════════════════════════════════════════════════════════════════════════════
#  STATUS FUNCTIONS
# ══════════════════════════════════════════════════════════════════════════════

# Print a status symbol (✓ for enabled, · for disabled)
# Uses manual padding to handle Unicode character width correctly
print_status() {
    local enabled="$1"
    local width="${2:-6}"
    local padding=$((width - 1))  # 1 char for symbol
    if [ "$enabled" -eq 1 ]; then
        printf "%${padding}s${GREEN}✓${NC}" ""
    else
        printf "%${padding}s${RED}·${NC}" ""
    fi
}

# Show status of all boards
show_status() {
    # Table dimensions - must match exactly between headers and data
    local board_w=14
    local col_w=6    # Each status column width

    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC}${BOLD}                FEB Library Integration Status                        ${NC}${CYAN}║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════════╝${NC}"
    echo ""

    # Section headers - centered above their columns
    printf "%-${board_w}s │" ""
    printf "        LIBRARIES         │"
    printf "         HAL MODULES"
    echo ""

    # Column headers using same widths as data
    printf "${BOLD}%-${board_w}s${NC} │" "Board"
    printf "%${col_w}s" "IO"
    printf "%${col_w}s" "CAN"
    printf "%${col_w}s" "TPS"
    printf "%${col_w}s" "RTOS"
    printf "  │"
    printf "%${col_w}s" "FRTOS"
    printf "%${col_w}s" "UART"
    printf "%${col_w}s" "I2C"
    printf "%${col_w}s" "CAN"
    echo ""

    # Separator line
    printf "%.0s─" $(seq 1 $board_w)
    printf "─┼"
    printf "%.0s─" $(seq 1 $((col_w * 4 + 2)))
    printf "─┼"
    printf "%.0s─" $(seq 1 $((col_w * 4)))
    echo ""

    for board in "${BOARDS[@]}"; do
        local cmake_file="$REPO_ROOT/$board/CMakeLists.txt"

        if [ ! -f "$cmake_file" ]; then
            printf "%-${board_w}s │ (no CMakeLists.txt)\n" "$board"
            continue
        fi

        # Check each status
        local io=0 can=0 tps=0 rtos=0 frtos=0 uart=0 i2c=0 can_hal=0

        has_lib "$cmake_file" "feb_io" && io=1
        has_can_sources "$cmake_file" && can=1
        has_lib "$cmake_file" "feb_tps" && tps=1
        has_lib "$cmake_file" "feb_rtos_utils" && rtos=1
        has_freertos "$board" && frtos=1
        has_uart_hal "$board" && uart=1
        has_i2c_hal "$board" && i2c=1
        has_can_hal "$board" && can_hal=1

        # Print row
        printf "%-${board_w}s │" "$board"
        print_status $io $col_w
        print_status $can $col_w
        print_status $tps $col_w
        print_status $rtos $col_w
        printf "  │"
        print_status $frtos $col_w
        print_status $uart $col_w
        print_status $i2c $col_w
        print_status $can_hal $col_w
        echo ""
    done

    echo ""
    echo -e "${BOLD}Legend:${NC}"
    echo "  IO=feb_io   CAN=feb_can   TPS=feb_tps   RTOS=feb_rtos_utils"
    echo "  FRTOS=FreeRTOS middleware"
    echo ""
    echo -e "${BOLD}Requirements:${NC} IO→UART   TPS→I2C   CAN→CAN"
}

# ══════════════════════════════════════════════════════════════════════════════
#  HELP
# ══════════════════════════════════════════════════════════════════════════════

show_help() {
    cat << EOF
FEB Firmware Library Configuration Script

Configures CMakeLists.txt to integrate common FEB libraries (TPS, CAN, UART/Log/Console).

Usage:
  ./scripts/configure-libs.sh -b BOARD [options]
  ./scripts/configure-libs.sh --list

Options:
  -b, --board BOARD    Board to configure (required for configuration)
  -l, --list           List boards and their current library status
  --dry-run            Preview changes without modifying files
  -h, --help           Show this help message

Library Selection (default: --all):
  --io                 Add feb_io (full UART/Log/Console stack)
  --can                Add CAN library (sources + includes)
  --tps                Add feb_tps (TPS2482 power monitoring)
  --rtos-utils         Add feb_rtos_utils
  --all                Add all libraries

FreeRTOS Options:
  --freertos           Force enable FreeRTOS mode (add compile definition)
  --no-freertos        Force disable FreeRTOS mode
                       (auto-detected from Middlewares/ directory by default)

Available Boards: ${BOARDS[*]}

Examples:
  ./scripts/configure-libs.sh -b DCU --all          # Full integration
  ./scripts/configure-libs.sh -b DCU --io --can     # Just I/O + CAN
  ./scripts/configure-libs.sh -b DCU --dry-run      # Preview changes
  ./scripts/configure-libs.sh --list                # Show current status

After configuration, build with:
  ./scripts/build.sh -b BOARD
EOF
}

# ══════════════════════════════════════════════════════════════════════════════
#  MAIN
# ══════════════════════════════════════════════════════════════════════════════

main() {
    local board=""
    local dry_run=false
    local show_list=false
    local add_io=false
    local add_can=false
    local add_tps=false
    local add_rtos_utils=false
    local add_all=false
    local force_freertos=""  # empty = auto, true = force on, false = force off

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -b|--board)
                if [ -z "$2" ] || [[ "$2" == -* ]]; then
                    log_error "Option -b requires a board name"
                    exit 1
                fi
                board="$2"
                shift 2
                ;;
            -l|--list)
                show_list=true
                shift
                ;;
            --dry-run)
                dry_run=true
                shift
                ;;
            --io)
                add_io=true
                shift
                ;;
            --can)
                add_can=true
                shift
                ;;
            --tps)
                add_tps=true
                shift
                ;;
            --rtos-utils)
                add_rtos_utils=true
                shift
                ;;
            --all)
                add_all=true
                shift
                ;;
            --freertos)
                force_freertos=true
                shift
                ;;
            --no-freertos)
                force_freertos=false
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
                # Allow board name as positional argument
                if [ -z "$board" ]; then
                    board="$1"
                else
                    log_error "Unexpected argument: $1"
                    exit 1
                fi
                shift
                ;;
        esac
    done

    # Show list and exit if requested
    if [ "$show_list" = true ]; then
        show_status
        exit 0
    fi

    # Validate board
    if [ -z "$board" ]; then
        log_error "Board name required. Use -b BOARD or --list to see available boards."
        echo ""
        show_help
        exit 1
    fi

    local valid_board=false
    for b in "${BOARDS[@]}"; do
        if [ "$b" = "$board" ]; then
            valid_board=true
            break
        fi
    done

    if [ "$valid_board" = false ]; then
        log_error "Unknown board: $board"
        echo "Valid boards: ${BOARDS[*]}"
        exit 1
    fi

    local cmake_file="$REPO_ROOT/$board/CMakeLists.txt"

    if [ ! -f "$cmake_file" ]; then
        log_error "CMakeLists.txt not found: $cmake_file"
        exit 1
    fi

    # Default to --all if no specific libraries selected
    if [ "$add_io" = false ] && [ "$add_can" = false ] && [ "$add_tps" = false ] && [ "$add_rtos_utils" = false ]; then
        add_all=true
    fi

    if [ "$add_all" = true ]; then
        add_io=true
        add_can=true
        add_tps=true
        add_rtos_utils=true
    fi

    # Determine FreeRTOS status
    local use_freertos=false
    if [ "$force_freertos" = true ]; then
        use_freertos=true
    elif [ "$force_freertos" = false ]; then
        use_freertos=false
    else
        # Auto-detect
        if has_freertos "$board"; then
            use_freertos=true
        fi
    fi

    log_header "Configuring $board"

    if [ "$dry_run" = true ]; then
        log_info "Dry run mode - no changes will be made"
        echo ""
    fi

    log_info "Board: $board"
    log_info "FreeRTOS: $([ "$use_freertos" = true ] && echo "Yes (detected)" || echo "No")"
    log_info "Libraries: IO=$add_io CAN=$add_can TPS=$add_tps RTOS-Utils=$add_rtos_utils"
    echo ""

    # Create backup (unless dry run)
    if [ "$dry_run" = false ]; then
        local backup_file="${cmake_file}.bak"
        cp "$cmake_file" "$backup_file"
        log_info "Created backup: $backup_file"
    fi

    # Check HAL prerequisites and collect warnings
    local hal_warnings=()

    if [ "$add_io" = true ] && ! has_uart_hal "$board"; then
        hal_warnings+=("UART HAL not enabled - feb_io library requires UART!")
    fi
    if [ "$add_tps" = true ] && ! has_i2c_hal "$board"; then
        hal_warnings+=("I2C HAL not enabled - feb_tps library requires I2C!")
    fi
    if [ "$add_can" = true ] && ! has_can_hal "$board"; then
        hal_warnings+=("CAN HAL not enabled - CAN library requires CAN!")
    fi

    # Show warnings if any
    if [ ${#hal_warnings[@]} -gt 0 ]; then
        for warning in "${hal_warnings[@]}"; do
            log_warn "$warning"
        done
        log_warn "Enable required HAL modules in STM32CubeMX and regenerate code."
        if [ "$dry_run" = false ]; then
            echo ""
            read -p "Continue anyway? [y/N] " -n 1 -r
            echo ""
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                log_info "Aborting. Restoring backup..."
                mv "${cmake_file}.bak" "$cmake_file"
                exit 1
            fi
        fi
        echo ""
    fi

    # Apply modifications
    local changes_made=false

    # FreeRTOS-related modifications
    if [ "$use_freertos" = true ]; then
        add_freertos_define "$cmake_file" "$dry_run" && changes_made=true
        add_freertos_sources "$cmake_file" "$dry_run" && changes_made=true
        add_freertos_includes "$cmake_file" "$dry_run" && changes_made=true
    fi

    # CAN library
    if [ "$add_can" = true ]; then
        add_can_sources "$cmake_file" "$dry_run" && changes_made=true
        add_can_includes "$cmake_file" "$dry_run" && changes_made=true
    fi

    # Update link libraries
    update_link_libraries "$cmake_file" "$dry_run" "$add_io" "$add_can" "$add_tps" "$add_rtos_utils" && changes_made=true

    echo ""

    if [ "$dry_run" = true ]; then
        log_info "Dry run complete. Run without --dry-run to apply changes."
    else
        log_info "Configuration complete!"
        echo ""
        echo "Next steps:"
        echo "  1. Review changes: git diff $board/CMakeLists.txt"
        echo "  2. Build: ./scripts/build.sh -b $board"
        echo ""
        echo "To revert: mv ${cmake_file}.bak $cmake_file"
    fi
}

main "$@"
