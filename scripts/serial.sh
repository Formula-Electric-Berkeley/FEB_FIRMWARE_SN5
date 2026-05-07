#!/bin/bash
#
# STM32 Board Serial Console Helper
#
# Opens the ST-Link VCP (or any USB serial adapter) at 115200 8N1 so you can
# verify a freshly-flashed board is actually alive — type `version`, `uptime`,
# `help` to round-trip the feb_io console. macOS-aware (prefers /dev/cu.* over
# /dev/tty.*); also works on Linux.
#
# Companion to scripts/flash.sh. Kept composable — does NOT auto-chain after
# flashing. Two-step pattern (documented in /CLAUDE.md):
#
#   ./scripts/flash.sh  -b BMS
#   ./scripts/serial.sh -b BMS
#
# Usage:
#   ./scripts/serial.sh                # Interactive: list ports, pick one
#   ./scripts/serial.sh -b BMS         # Same, plus prints expected boot output
#   ./scripts/serial.sh -p /dev/cu.usbmodem1234   # Open a specific port
#   ./scripts/serial.sh --baud 9600    # Override baud rate (default 115200)
#   ./scripts/serial.sh --list         # List candidate ports and exit
#   ./scripts/serial.sh -h             # Show help
#
set -e

# Same board list as flash.sh — keep these in sync if a board is added.
BOARDS=("BMS" "DASH" "DART" "DCU" "LVPDB" "PCU" "Sensor_Nodes_FRONT" "Sensor_Nodes_REAR" "UART" "UART_TEST")

DEFAULT_BAUD=115200

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

log_info()   { echo -e "${GREEN}[INFO]${NC} $1" >&2; }
log_warn()   { echo -e "${YELLOW}[WARN]${NC} $1" >&2; }
log_error()  { echo -e "${RED}[ERROR]${NC} $1" >&2; }
log_header() { echo -e "\n${BLUE}=== $1 ===${NC}\n" >&2; }

get_script_dir() { cd "$(dirname "$0")" && pwd; }

# Per-board hint shown before opening serial. Sourced from each board's
# CLAUDE.md "Verifying after flash" section. Keep it short — full details
# live in the per-board CLAUDE.md.
expected_banner_for() {
    case "$1" in
        BMS)
            echo "Expect within ~2s: '[BOOT] UART ready @115200' then '[...] I MAIN: BMS initialization complete' then 'BMS Console Ready' banner."
            echo "Smoke check: type \`version\`, \`uptime\`, \`help\`, \`bms|status\`."
            ;;
        DASH)
            echo "Expect: minimal serial output (display/LVGL is the primary UI). UART is available but not the primary signal."
            echo "Smoke check: \`version\`, \`uptime\`. Real proof-of-life is the LCD coming up + CAN frames."
            ;;
        DART)
            echo "Expect: silence (Cortex-M0, 32 KB Flash, no startup banner). Bespoke command parser, NOT feb_commands."
            echo "Real proof-of-life is on CAN — look for tachometer frames. Flag the parser divergence if you touch this code."
            ;;
        DCU)
            echo "Expect: nothing (placeholder skeleton; Core/User/ is empty). USART2 is enabled in the .ioc but no app code drives it."
            echo "If you're trying to verify behavior here, there is no behavior yet — propose what should be added."
            ;;
        LVPDB)
            echo "Expect: no FEB_Main banner (bare-metal, init driven from CubeMX-generated main.c). feb_io may or may not be wired."
            echo "Primary proof-of-life is on CAN (dual-CAN, 7× TPS2482 monitors). Check CAN bus before declaring success."
            ;;
        PCU)
            echo "Expect within ~2-3s: 8 init checkpoints ('[1/8] UART...' through '=== PCU Setup Complete ==='). Watch for 'DEGRADED (CAN FAILED)' on line 8."
            echo "Smoke check: \`version\`, \`uptime\`. Real validation is on CAN2 (RMS inverter gateway)."
            ;;
        Sensor_Nodes_FRONT|Sensor_Nodes_REAR|Sensor_Nodes)
            echo "Expect: quiet by default (bare-metal polling). GPS/IMU/WSS data goes out on CAN, not UART."
            echo "Smoke check: type any text — if RX task is alive, it processes input. Real validation is CAN frames."
            ;;
        UART)
            echo "Expect within ~1s: 'UART Console Ready' banner with prompt. This is the reference board for the feb_io serial stack."
            echo "Smoke check: \`help\` (lists all built-in commands), \`echo|hello world\` (round-trip), \`version\`, \`uptime\`."
            ;;
        UART_TEST)
            echo "Expect: 'UART Test Before FreeRTOS' early, then full banner after the scheduler starts. STM32U5 (Cortex-M33) validation fixture."
            echo "Smoke check: \`version\`, \`uptime\`, \`help\`. Confirms the serial stack works on the next-gen platform."
            ;;
        *)
            echo "Unknown board — no boot expectations on file. Connect anyway and look for any output at $DEFAULT_BAUD baud."
            ;;
    esac
}

is_valid_board() {
    local board="$1"
    for b in "${BOARDS[@]}"; do
        [ "$b" = "$board" ] && return 0
    done
    return 1
}

# Discover serial ports. macOS prefers /dev/cu.* (call-unit) over /dev/tty.*
# (which blocks on DCD); Linux uses /dev/ttyUSB* and /dev/ttyACM*.
list_ports() {
    case "$(uname -s)" in
        Darwin*)
            ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART* 2>/dev/null || true
            ;;
        Linux*)
            ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
            ;;
        *)
            ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
            ;;
    esac
}

# Pick a terminal program: tio > picocom > screen > minicom.
# tio is the nicest of the four (no quoting, sane defaults, /dev autoreconnect).
pick_terminal() {
    for prog in tio picocom screen minicom; do
        if command -v "$prog" &> /dev/null; then
            echo "$prog"
            return 0
        fi
    done
    return 1
}

show_install_hint() {
    log_header "No serial terminal program found"
    case "$(uname -s)" in
        Darwin*)
            echo "Install one of:"
            echo "  brew install tio        # recommended"
            echo "  brew install picocom"
            echo "  # screen and minicom are usually preinstalled"
            ;;
        Linux*)
            echo "Install one of:"
            echo "  sudo apt install tio        # recommended (Debian/Ubuntu)"
            echo "  sudo apt install picocom"
            echo "  sudo apt install screen"
            echo "  sudo apt install minicom"
            ;;
        *)
            echo "Install tio, picocom, screen, or minicom from your package manager."
            ;;
    esac
}

# Print the detach-key hint for whichever terminal we're about to launch.
# Embedded boards lock up your terminal until you know how to escape.
print_detach_hint() {
    local prog="$1"
    case "$prog" in
        tio)      log_info "Detach: Ctrl-T q   |   List shortcuts: Ctrl-T ?" ;;
        picocom)  log_info "Detach: Ctrl-A Ctrl-X" ;;
        screen)   log_info "Detach: Ctrl-A k (kill) or Ctrl-A d (detach)" ;;
        minicom)  log_info "Detach: Ctrl-A x" ;;
    esac
}

open_serial() {
    local port="$1"
    local baud="$2"

    if [ ! -e "$port" ]; then
        log_error "Port not found: $port"
        log_warn "Available ports right now:"
        local available
        available=$(list_ports)
        if [ -z "$available" ]; then
            log_warn "  (none) — is the board plugged in? Did you flash it?"
        else
            echo "$available" | sed 's/^/  /' >&2
        fi
        return 1
    fi

    local prog
    if ! prog=$(pick_terminal); then
        show_install_hint
        return 1
    fi

    log_info "Opening $port @ $baud using $prog"
    print_detach_hint "$prog"
    echo "" >&2

    case "$prog" in
        tio)
            # tio handles baud as -b; --no-autoconnect keeps behavior predictable.
            tio -b "$baud" "$port"
            ;;
        picocom)
            picocom -b "$baud" "$port"
            ;;
        screen)
            # screen wants baud as a positional after the port.
            screen "$port" "$baud"
            ;;
        minicom)
            # minicom: -D device, -b baud, -o no init string.
            minicom -D "$port" -b "$baud" -o
            ;;
    esac
}

interactive_pick_port() {
    local ports
    ports=$(list_ports)

    if [ -z "$ports" ]; then
        log_error "No serial ports found."
        log_warn "Plug in the board (USB cable from ST-Link/VCP). On macOS, expect /dev/cu.usbmodem*"
        log_warn "If the board is plugged in but not showing up, the ST-Link VCP driver may need updating."
        return 1
    fi

    echo -e "${BOLD}Select a serial port:${NC}" >&2
    echo "" >&2

    local -a port_list=()
    local i=1
    while IFS= read -r p; do
        port_list+=("$p")
        printf "  %d) %s\n" "$i" "$p" >&2
        i=$((i + 1))
    done <<< "$ports"

    echo "" >&2
    echo "  q) Quit" >&2
    echo "" >&2

    read -p "Enter selection: " selection
    case "$selection" in
        q|Q|quit|exit) return 1 ;;
        [0-9]|[0-9][0-9])
            local idx=$((selection - 1))
            if [ "$idx" -ge 0 ] && [ "$idx" -lt "${#port_list[@]}" ]; then
                echo "${port_list[$idx]}"
                return 0
            fi
            log_error "Invalid selection: $selection"
            return 1
            ;;
        *)
            if [ -e "$selection" ]; then
                echo "$selection"
                return 0
            fi
            log_error "Invalid selection: $selection"
            return 1
            ;;
    esac
}

show_help() {
    echo "FEB Firmware Serial Console Helper"
    echo ""
    echo "Usage: ./scripts/serial.sh [options]"
    echo ""
    echo "Options:"
    echo "  -b, --board <BOARD>    Print expected boot output for BOARD, then open serial"
    echo "  -p, --port <PATH>      Open a specific serial port directly"
    echo "      --baud <N>         Override baud rate (default $DEFAULT_BAUD)"
    echo "      --list             List candidate serial ports and exit"
    echo "  -h, --help             Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./scripts/serial.sh                     # Interactive: pick a port"
    echo "  ./scripts/serial.sh -b BMS              # Print BMS boot expectations + interactive"
    echo "  ./scripts/serial.sh -p /dev/cu.usbmodem1234"
    echo "  ./scripts/serial.sh --baud 9600 -b DART"
    echo "  ./scripts/serial.sh --list              # Just list ports"
    echo ""
    echo "Boards with boot expectations on file:"
    echo "  ${BOARDS[*]}"
    echo ""
    echo "Terminal program preference (first found wins):"
    echo "  tio > picocom > screen > minicom"
    echo ""
    echo "Note:"
    echo "  After flashing, ALWAYS use this script (or your own terminal program)"
    echo "  to verify the board is actually alive. 'Flash succeeded' is not proof"
    echo "  the firmware works — \`version\` / \`uptime\` / \`help\` over serial is."
}

main() {
    local board=""
    local port=""
    local baud="$DEFAULT_BAUD"
    local list_only=false

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -b|--board)
                board="$2"
                shift 2
                ;;
            -p|--port)
                port="$2"
                shift 2
                ;;
            --baud)
                baud="$2"
                shift 2
                ;;
            --list)
                list_only=true
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

    if [ "$list_only" = true ]; then
        log_header "Serial ports"
        local ports
        ports=$(list_ports)
        if [ -z "$ports" ]; then
            echo "(none)"
        else
            echo "$ports"
        fi
        exit 0
    fi

    log_header "FEB Serial Console"

    # If a board was named, print what to expect before opening the line —
    # so the user (and Claude) knows whether what they're seeing is correct.
    if [ -n "$board" ]; then
        if ! is_valid_board "$board" && [ "$board" != "Sensor_Nodes" ]; then
            log_warn "Unknown board: $board (continuing anyway)"
        fi
        log_info "Board: $board"
        echo "" >&2
        expected_banner_for "$board" | sed 's/^/  /' >&2
        echo "" >&2
    fi

    # Pick a port if one wasn't supplied.
    if [ -z "$port" ]; then
        if ! port=$(interactive_pick_port); then
            exit 1
        fi
    fi

    open_serial "$port" "$baud"
}

main "$@"
