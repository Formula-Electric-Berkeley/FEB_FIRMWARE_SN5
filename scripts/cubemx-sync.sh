#!/bin/bash
#
# CubeMX Code Sync Validation Script
#
# Ensures CubeMX-generated code stays in sync with .ioc files by tracking
# checksums in a manifest file. Works in pre-commit hooks and CI without
# requiring CubeMX installation.
#
# Usage:
#   ./scripts/cubemx-sync.sh --status            # Show sync status
#   ./scripts/cubemx-sync.sh --update            # Update manifest (all boards)
#   ./scripts/cubemx-sync.sh --update -b BMS     # Update manifest (single board)
#   ./scripts/cubemx-sync.sh --validate          # CI: verify manifest matches files
#   ./scripts/cubemx-sync.sh --check             # Pre-commit: verify current state matches manifest
#   ./scripts/cubemx-sync.sh -h                  # Show help
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."
MANIFEST_FILE="$REPO_ROOT/.cubemx-manifest.json"

# Available boards (same as cubemx.sh)
BOARDS=("BMS" "DASH" "DART" "DCU" "LVPDB" "PCU" "Sensor_Nodes" "UART_TEST")

# IOC metadata patterns to EXCLUDE from hash (GUI-only, don't affect code generation)
EXCLUDE_PATTERNS=(
    "^PinOutPanel\."
    "^CAD\."
    "^PCC\.Display"
    "^PCC\.Checker"
    "^KeepUserPlacement"
    "^GPIO\.groupedBy"
    "^Mcu\.Context[0-9]"
    "^ProjectManager\.BackupPrev498Gen"
    "^ProjectManager\.CoupleFile"
    "^ProjectManager\.DeletePrevGen"
    "^ProjectManager\.FreePins"
    "^ProjectManager\.KeepUserCode"
    "^ProjectManager\.PreviousUsedFiles"
)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

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

# Compute normalized checksum of .ioc file (excluding GUI-only metadata)
compute_ioc_checksum() {
    local ioc_file="$1"
    local exclude_regex
    exclude_regex=$(IFS='|'; echo "${EXCLUDE_PATTERNS[*]}")

    grep -v -E "$exclude_regex" "$ioc_file" 2>/dev/null | \
        LC_ALL=C sort | \
        shasum -a 256 | \
        cut -d' ' -f1
}

# Compute checksum of a file
compute_file_checksum() {
    local file="$1"
    shasum -a 256 "$file" 2>/dev/null | cut -d' ' -f1
}

# Check if peripheral source files match enabled HAL modules
# Returns number of missing files (0 = all good)
check_peripheral_files() {
    local board="$1"
    local board_dir="$REPO_ROOT/$board"
    local src_dir="$board_dir/Core/Src"
    local missing=0

    # Find HAL config file (supports F4 and other series)
    local hal_conf=""
    for conf in "$board_dir/Core/Inc/stm32"*"_hal_conf.h"; do
        if [ -f "$conf" ]; then
            hal_conf="$conf"
            break
        fi
    done

    if [ ! -f "$hal_conf" ]; then
        return 0
    fi

    # Peripheral to source file mapping
    # Format: "HAL_MODULE_PATTERN:source_file"
    local peripherals=(
        "HAL_UART_MODULE_ENABLED:usart.c"
        "HAL_USART_MODULE_ENABLED:usart.c"
        "HAL_CAN_MODULE_ENABLED:can.c"
        "HAL_FDCAN_MODULE_ENABLED:fdcan.c"
        "HAL_I2C_MODULE_ENABLED:i2c.c"
        "HAL_SPI_MODULE_ENABLED:spi.c"
        "HAL_ADC_MODULE_ENABLED:adc.c"
        "HAL_TIM_MODULE_ENABLED:tim.c"
        "HAL_DMA_MODULE_ENABLED:dma.c"
        "HAL_DAC_MODULE_ENABLED:dac.c"
        "HAL_RTC_MODULE_ENABLED:rtc.c"
        "HAL_SDIO_MODULE_ENABLED:sdio.c"
        "HAL_SD_MODULE_ENABLED:sdio.c"
        "HAL_SAI_MODULE_ENABLED:sai.c"
        "HAL_QSPI_MODULE_ENABLED:quadspi.c"
        "HAL_LTDC_MODULE_ENABLED:ltdc.c"
        "HAL_DSI_MODULE_ENABLED:dsihost.c"
        "HAL_DMA2D_MODULE_ENABLED:dma2d.c"
        "HAL_CRC_MODULE_ENABLED:crc.c"
        "HAL_FMC_MODULE_ENABLED:fmc.c"
    )

    for mapping in "${peripherals[@]}"; do
        local hal_module="${mapping%%:*}"
        local src_file="${mapping##*:}"

        # Check if HAL module is enabled (uncommented #define)
        if grep -q "^#define $hal_module" "$hal_conf" 2>/dev/null; then
            if [ ! -f "$src_dir/$src_file" ]; then
                # Only error for UART/USART since those are commonly configured explicitly
                # Other HAL modules may be enabled for internal use (e.g., TIM for FreeRTOS)
                if [[ "$hal_module" == *"UART"* ]] || [[ "$hal_module" == *"USART"* ]]; then
                    log_error "$board: $hal_module enabled but $src_file missing"
                    log_error "  Regenerate with: ./scripts/cubemx.sh -g -b $board"
                    ((missing++))
                fi
            fi
        fi
    done

    return $missing
}

# Check if a file is CubeMX-generated (has USER CODE markers or @author marker)
is_cubemx_generated() {
    local file="$1"
    grep -q -E "(USER CODE BEGIN|@author.*STM32CubeMX|Auto-generated)" "$file" 2>/dev/null
}

# Find all CubeMX-generated files for a board
find_generated_files() {
    local board="$1"
    local board_dir="$REPO_ROOT/$board"
    local files=()

    # Core/Inc headers
    if [ -d "$board_dir/Core/Inc" ]; then
        while IFS= read -r -d '' file; do
            if is_cubemx_generated "$file"; then
                files+=("${file#$board_dir/}")
            fi
        done < <(find "$board_dir/Core/Inc" -name "*.h" -print0 2>/dev/null)
    fi

    # Core/Src sources (exclude User directory)
    if [ -d "$board_dir/Core/Src" ]; then
        while IFS= read -r -d '' file; do
            # Skip User directory
            if [[ "$file" == *"/User/"* ]]; then
                continue
            fi
            if is_cubemx_generated "$file"; then
                files+=("${file#$board_dir/}")
            fi
        done < <(find "$board_dir/Core/Src" -name "*.c" -print0 2>/dev/null)
    fi

    # Middlewares (FreeRTOS, etc.) - track .c and .h files
    if [ -d "$board_dir/Middlewares" ]; then
        while IFS= read -r -d '' file; do
            files+=("${file#$board_dir/}")
        done < <(find "$board_dir/Middlewares" \( -name "*.c" -o -name "*.h" \) -print0 2>/dev/null)
    fi

    # Linker scripts (*.ld files in board root)
    while IFS= read -r -d '' file; do
        files+=("${file#$board_dir/}")
    done < <(find "$board_dir" -maxdepth 1 -name "*.ld" -print0 2>/dev/null)

    # Startup assembly files (startup_*.s in board root)
    while IFS= read -r -d '' file; do
        files+=("${file#$board_dir/}")
    done < <(find "$board_dir" -maxdepth 1 -name "startup_*.s" -print0 2>/dev/null)

    # Sort for consistent ordering
    printf '%s\n' "${files[@]}" | LC_ALL=C sort
}

# Read manifest file and extract board data
read_manifest() {
    if [ ! -f "$MANIFEST_FILE" ]; then
        echo "{}"
        return
    fi
    cat "$MANIFEST_FILE"
}

# Get board data from manifest JSON using grep/sed (portable, no jq dependency)
get_manifest_board_ioc_checksum() {
    local manifest="$1"
    local board="$2"
    echo "$manifest" | grep -A1 "\"$board\"" | grep "ioc_checksum" | sed 's/.*"ioc_checksum": *"\([^"]*\)".*/\1/' | head -1
}

# Write manifest file
write_manifest() {
    local content="$1"
    echo "$content" > "$MANIFEST_FILE"
}

# Update manifest for a single board (outputs JSON to stdout, logs to stderr)
update_board_manifest() {
    local board="$1"
    local ioc_file
    ioc_file=$(get_ioc_path "$board")
    local board_dir="$REPO_ROOT/$board"

    if [ ! -f "$ioc_file" ]; then
        log_warn "Skipping $board (no .ioc file)" >&2
        return 1
    fi

    log_info "Processing $board..." >&2

    # Compute IOC checksum
    local ioc_checksum
    ioc_checksum=$(compute_ioc_checksum "$ioc_file")

    # Find and checksum generated files
    local files_json=""
    local first=true
    while IFS= read -r rel_path; do
        [ -z "$rel_path" ] && continue
        local full_path="$board_dir/$rel_path"
        local file_checksum
        file_checksum=$(compute_file_checksum "$full_path")

        if [ "$first" = true ]; then
            first=false
        else
            files_json="$files_json,"
        fi
        files_json="$files_json
        \"$rel_path\": \"$file_checksum\""
    done < <(find_generated_files "$board")

    local timestamp
    timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    # Return board JSON block (proper JSON formatting)
    cat << EOF
    "$board": {
      "ioc_checksum": "$ioc_checksum",
      "generated_at": "$timestamp",
      "files": {$files_json
      }
    }
EOF
}

# Update entire manifest
update_manifest() {
    local target_board="$1"

    log_header "Updating CubeMX Manifest"

    local boards_json=""
    local first=true

    # Always regenerate full manifest to ensure consistency
    for board in "${BOARDS[@]}"; do
        if ! has_ioc_file "$board"; then
            continue
        fi

        local board_json
        board_json=$(update_board_manifest "$board")

        if [ "$first" = true ]; then
            first=false
            boards_json="
$board_json"
        else
            boards_json="$boards_json,
$board_json"
        fi
    done

    local manifest_content="{
  \"version\": 1,
  \"description\": \"CubeMX code sync manifest - do not edit manually\",
  \"boards\": {$boards_json
  }
}"

    write_manifest "$manifest_content"
    log_info "Manifest updated: $MANIFEST_FILE"
}

# Validate manifest against current files
validate_manifest() {
    log_header "Validating CubeMX Manifest"

    if [ ! -f "$MANIFEST_FILE" ]; then
        log_error "Manifest file not found: $MANIFEST_FILE"
        log_error "Run './scripts/cubemx-sync.sh --update' to create it"
        return 1
    fi

    local errors=0
    local warnings=0

    for board in "${BOARDS[@]}"; do
        if ! has_ioc_file "$board"; then
            continue
        fi

        local ioc_file
        ioc_file=$(get_ioc_path "$board")
        local board_dir="$REPO_ROOT/$board"

        echo -e "${CYAN}Checking $board...${NC}"

        # Compute current IOC checksum
        local current_ioc_checksum
        current_ioc_checksum=$(compute_ioc_checksum "$ioc_file")

        # Get manifest IOC checksum (using Python for reliable JSON parsing)
        local manifest_ioc_checksum
        manifest_ioc_checksum=$(python3 -c "
import json
with open('$MANIFEST_FILE') as f:
    m = json.load(f)
print(m.get('boards', {}).get('$board', {}).get('ioc_checksum', ''))
" 2>/dev/null || echo "")

        if [ -z "$manifest_ioc_checksum" ]; then
            log_error "  Board '$board' not found in manifest"
            ((errors++))
            continue
        fi

        if [ "$current_ioc_checksum" != "$manifest_ioc_checksum" ]; then
            log_error "  IOC checksum mismatch!"
            log_error "    Current:  $current_ioc_checksum"
            log_error "    Manifest: $manifest_ioc_checksum"
            log_error "  Run: ./scripts/cubemx.sh -g -b $board && ./scripts/cubemx-sync.sh --update -b $board"
            ((errors++))
            continue
        fi

        # Validate generated files (strict mode)
        local file_errors=0
        while IFS= read -r rel_path; do
            [ -z "$rel_path" ] && continue
            local full_path="$board_dir/$rel_path"
            local current_checksum
            current_checksum=$(compute_file_checksum "$full_path")

            local manifest_checksum
            manifest_checksum=$(python3 -c "
import json
with open('$MANIFEST_FILE') as f:
    m = json.load(f)
print(m.get('boards', {}).get('$board', {}).get('files', {}).get('$rel_path', ''))
" 2>/dev/null || echo "")

            if [ -z "$manifest_checksum" ]; then
                log_error "  File not in manifest: $rel_path"
                log_error "  Run: ./scripts/cubemx-sync.sh --update -b $board"
                ((file_errors++))
            elif [ "$current_checksum" != "$manifest_checksum" ]; then
                log_error "  File modified: $rel_path"
                ((file_errors++))
            fi
        done < <(find_generated_files "$board")

        # Check manifest files exist on disk (detect deletions)
        local manifest_files
        manifest_files=$(python3 -c "
import json
with open('$MANIFEST_FILE') as f:
    m = json.load(f)
files = m.get('boards', {}).get('$board', {}).get('files', {})
for f in files:
    print(f)
" 2>/dev/null || echo "")

        while IFS= read -r rel_path; do
            [ -z "$rel_path" ] && continue
            local full_path="$board_dir/$rel_path"
            if [ ! -f "$full_path" ]; then
                log_error "  Missing file (in manifest but not on disk): $rel_path"
                log_error "  Regenerate with: ./scripts/cubemx.sh -g -b $board && ./scripts/cubemx-sync.sh --update -b $board"
                ((file_errors++))
            fi
        done <<< "$manifest_files"

        # Check for missing peripheral source files (HAL enabled but source missing)
        local peripheral_errors=0
        check_peripheral_files "$board"
        peripheral_errors=$?
        if [ $peripheral_errors -gt 0 ]; then
            ((file_errors += peripheral_errors))
        fi

        if [ $file_errors -gt 0 ]; then
            log_error "  $file_errors generated file(s) differ from manifest"
            log_error "  Regenerate with: ./scripts/cubemx.sh -g -b $board && ./scripts/cubemx-sync.sh --update -b $board"
            ((errors++))
        else
            log_info "  OK"
        fi
    done

    echo ""
    if [ $errors -gt 0 ]; then
        log_error "Validation failed with $errors error(s)"

        # GitHub Actions summary
        if [ -n "$GITHUB_STEP_SUMMARY" ]; then
            echo "## CubeMX Sync Validation Failed" >> "$GITHUB_STEP_SUMMARY"
            echo "" >> "$GITHUB_STEP_SUMMARY"
            echo "Found $errors board(s) out of sync with manifest." >> "$GITHUB_STEP_SUMMARY"
            echo "" >> "$GITHUB_STEP_SUMMARY"
            echo "To fix: Regenerate code with \`./scripts/cubemx.sh -g -b BOARD\` and update manifest with \`./scripts/cubemx-sync.sh --update\`" >> "$GITHUB_STEP_SUMMARY"
        fi

        return 1
    fi

    log_info "All boards validated successfully"

    if [ -n "$GITHUB_STEP_SUMMARY" ]; then
        echo "## CubeMX Sync Validation Passed" >> "$GITHUB_STEP_SUMMARY"
        echo "" >> "$GITHUB_STEP_SUMMARY"
        echo "All boards are in sync with their .ioc files." >> "$GITHUB_STEP_SUMMARY"
    fi

    return 0
}

# Pre-commit check: validate current state matches manifest
check_staged() {
    # Just run full validation - ensures all boards are in sync
    validate_manifest
}

# Show sync status for all boards
show_status() {
    log_header "CubeMX Sync Status"

    if [ ! -f "$MANIFEST_FILE" ]; then
        log_warn "Manifest file not found: $MANIFEST_FILE"
        log_warn "Run './scripts/cubemx-sync.sh --update' to create it"
        echo ""
    fi

    printf "%-15s %-12s %-12s %s\n" "Board" "IOC" "Files" "Status"
    printf "%-15s %-12s %-12s %s\n" "-----" "---" "-----" "------"

    for board in "${BOARDS[@]}"; do
        local ioc_file
        ioc_file=$(get_ioc_path "$board")
        local board_dir="$REPO_ROOT/$board"

        if [ ! -f "$ioc_file" ]; then
            printf "%-15s %-12s %-12s %b\n" "$board" "-" "-" "${YELLOW}no .ioc${NC}"
            continue
        fi

        # Check IOC sync
        local current_ioc_checksum
        current_ioc_checksum=$(compute_ioc_checksum "$ioc_file")

        local manifest_ioc_checksum=""
        if [ -f "$MANIFEST_FILE" ]; then
            manifest_ioc_checksum=$(python3 -c "
import json
with open('$MANIFEST_FILE') as f:
    m = json.load(f)
print(m.get('boards', {}).get('$board', {}).get('ioc_checksum', ''))
" 2>/dev/null || echo "")
        fi

        local ioc_status files_status overall_status

        if [ -z "$manifest_ioc_checksum" ]; then
            ioc_status="${YELLOW}untracked${NC}"
            files_status="-"
            overall_status="${YELLOW}not in manifest${NC}"
        elif [ "$current_ioc_checksum" = "$manifest_ioc_checksum" ]; then
            ioc_status="${GREEN}match${NC}"

            # Check generated files on disk against manifest
            local file_mismatches=0
            local total_files=0
            while IFS= read -r rel_path; do
                [ -z "$rel_path" ] && continue
                ((total_files++))
                local full_path="$board_dir/$rel_path"
                local current_checksum
                current_checksum=$(compute_file_checksum "$full_path")

                local manifest_checksum
                manifest_checksum=$(python3 -c "
import json
with open('$MANIFEST_FILE') as f:
    m = json.load(f)
print(m.get('boards', {}).get('$board', {}).get('files', {}).get('$rel_path', ''))
" 2>/dev/null || echo "")

                if [ "$current_checksum" != "$manifest_checksum" ]; then
                    ((file_mismatches++))
                fi
            done < <(find_generated_files "$board")

            # Check for missing files (in manifest but not on disk)
            local missing_files=0
            local manifest_files
            manifest_files=$(python3 -c "
import json
with open('$MANIFEST_FILE') as f:
    m = json.load(f)
files = m.get('boards', {}).get('$board', {}).get('files', {})
for f in files:
    print(f)
" 2>/dev/null || echo "")

            while IFS= read -r rel_path; do
                [ -z "$rel_path" ] && continue
                local full_path="$board_dir/$rel_path"
                if [ ! -f "$full_path" ]; then
                    ((missing_files++))
                fi
            done <<< "$manifest_files"

            if [ $file_mismatches -eq 0 ] && [ $missing_files -eq 0 ]; then
                files_status="${GREEN}${total_files} ok${NC}"
                overall_status="${GREEN}in sync${NC}"
            elif [ $missing_files -gt 0 ]; then
                files_status="${RED}${missing_files} missing${NC}"
                overall_status="${RED}files missing${NC}"
            else
                files_status="${RED}${file_mismatches}/${total_files}${NC}"
                overall_status="${RED}files modified${NC}"
            fi
        else
            ioc_status="${RED}changed${NC}"
            files_status="-"
            overall_status="${RED}needs regen${NC}"
        fi

        printf "%-15s %-12b %-12b %b\n" "$board" "$ioc_status" "$files_status" "$overall_status"
    done
}

# Show help
show_help() {
    cat << EOF
CubeMX Code Sync Validation Script

Ensures CubeMX-generated code stays in sync with .ioc files by tracking
checksums in a manifest file.

Usage:
  ./scripts/cubemx-sync.sh [command] [options]

Commands:
  --status            Show sync status for all boards
  --update            Update manifest for all boards
  --validate          Validate manifest matches current files (for CI)
  --check             Validate current state matches manifest (for pre-commit)
  -h, --help          Show this help message

Options:
  -b, --board BOARD   Target specific board (with --update)

Examples:
  ./scripts/cubemx-sync.sh --status              # See current sync state
  ./scripts/cubemx-sync.sh --update              # Update manifest (all boards)
  ./scripts/cubemx-sync.sh --update -b BMS       # Update manifest (BMS only)
  ./scripts/cubemx-sync.sh --update -b all       # Update manifest (all boards)
  ./scripts/cubemx-sync.sh --validate            # CI validation
  ./scripts/cubemx-sync.sh --check               # Pre-commit check

Workflow:
  1. Edit .ioc in CubeMX
  2. Generate code: ./scripts/cubemx.sh -g -b BOARD
  3. Update manifest: ./scripts/cubemx-sync.sh --update -b BOARD
  4. Commit all changes

Boards: ${BOARDS[*]} (or 'all')
EOF
}

# Main function
main() {
    local command=""
    local board=""

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --status)
                command="status"
                shift
                ;;
            --update)
                command="update"
                shift
                ;;
            --validate)
                command="validate"
                shift
                ;;
            --check)
                command="check"
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

    # Validate board if specified
    if [ -n "$board" ] && [ "$board" != "all" ]; then
        if ! is_valid_board "$board"; then
            log_error "Invalid board: $board"
            echo "Valid boards: ${BOARDS[*]} all"
            exit 1
        fi
    fi

    # Execute command
    case "$command" in
        status)
            show_status
            ;;
        update)
            update_manifest "$board"
            ;;
        validate)
            validate_manifest
            ;;
        check)
            check_staged
            ;;
        *)
            # Default to status
            show_status
            ;;
    esac
}

main "$@"
