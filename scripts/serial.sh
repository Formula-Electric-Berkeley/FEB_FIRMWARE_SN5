#!/bin/bash
#
# STM32 Board Serial Console Helper (tio wrapper)
#
# Three modes:
#   - Interactive (default)        — board menu → port menu → tio
#   - --read <SECONDS>             — capture N seconds of output, print, exit
#   - --exec "<cmd1,cmd2,...>"     — send commands, capture, exit
#
# Companion to scripts/flash.sh. After a flash, this is how you actually
# verify the board is alive — type `version` / `uptime` / `help` (or have
# Claude do it via --exec) over the feb_io console.
#
# Two-step pattern (kept composable, NOT auto-chained):
#
#   ./scripts/flash.sh   -b BMS
#   ./scripts/serial.sh  -b BMS                         # interactive
#   ./scripts/serial.sh  -b BMS --exec "version,uptime" # one-shot verify
#
# Usage:
#   ./scripts/serial.sh                                # Board menu → port
#   ./scripts/serial.sh -b BMS                         # Hints + port menu
#   ./scripts/serial.sh -p /dev/cu.usbmodem1234        # Direct open
#   ./scripts/serial.sh -b BMS --read 3                # 3s capture + exit
#   ./scripts/serial.sh -b BMS --exec "version,uptime" # round-trip + exit
#   ./scripts/serial.sh --list                         # List ports + exit
#   ./scripts/serial.sh -h
#
set -e

# Same board list as flash.sh — keep these in sync if a board is added.
BOARDS=("BMS" "DASH" "DART" "DCU" "LVPDB" "PCU" "Sensor_Nodes_FRONT" "Sensor_Nodes_REAR" "UART" "UART_TEST")

DEFAULT_BAUD=115200
DEFAULT_SETTLE=2

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# All log helpers write to stderr so $(...) capture in --exec / --read mode
# returns just the captured serial transcript, never diagnostics.
log_info()   { echo -e "${GREEN}[INFO]${NC} $1" >&2; }
log_warn()   { echo -e "${YELLOW}[WARN]${NC} $1" >&2; }
log_error()  { echo -e "${RED}[ERROR]${NC} $1" >&2; }
log_header() { echo -e "\n${BLUE}=== $1 ===${NC}\n" >&2; }

is_valid_board() {
    local board="$1"
    for b in "${BOARDS[@]}"; do
        [ "$b" = "$board" ] && return 0
    done
    return 1
}

check_prerequisites() {
    command -v tio &> /dev/null
}

show_setup_instructions() {
    log_header "tio not installed"
    echo -e "${BOLD}This script is a thin wrapper around \`tio\`.${NC}" >&2
    echo "" >&2
    case "$(uname -s)" in
        Darwin*)
            echo -e "${CYAN}macOS:${NC}  brew install tio" >&2
            ;;
        Linux*)
            echo -e "${CYAN}Debian/Ubuntu:${NC}  sudo apt install tio" >&2
            echo -e "${CYAN}Fedora/RHEL:${NC}    sudo dnf install tio" >&2
            echo -e "${CYAN}Arch:${NC}           sudo pacman -S tio" >&2
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo "Windows: download from https://github.com/tio/tio/releases" >&2
            ;;
        *)
            echo "Install tio from your package manager or https://github.com/tio/tio" >&2
            ;;
    esac
    echo "" >&2
    echo "After installing, re-run this script." >&2
}

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

# Short tag shown next to the board in the interactive menu. One-liner
# distilled from expected_banner_for so the menu fits on one screen.
board_hint_tag() {
    case "$1" in
        BMS)                 echo "expects banner" ;;
        DASH)                echo "LCD primary" ;;
        DART)                echo "silent / CAN-only" ;;
        DCU)                 echo "placeholder skeleton" ;;
        LVPDB)               echo "CAN primary" ;;
        PCU)                 echo "8-step boot trace" ;;
        Sensor_Nodes_FRONT)  echo "CAN primary" ;;
        Sensor_Nodes_REAR)   echo "CAN primary" ;;
        UART)                echo "reference board" ;;
        UART_TEST)           echo "STM32U5 fixture" ;;
        *)                   echo "" ;;
    esac
}

# Discover serial ports. macOS prefers /dev/cu.* (call-unit) over /dev/tty.*
# (which blocks on DCD). Glob expansion keeps each path on its own line; the
# `[ -e ]` guard discards literal-pattern matches when nothing connects.
# Explicit `return 0` because the last `[ -e ]` may be false under `set -e`,
# which would otherwise propagate through `ports=$(list_ports)` and abort.
list_ports() {
    local p
    case "$(uname -s)" in
        Darwin*)
            for p in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART*; do
                [ -e "$p" ] && echo "$p"
            done
            ;;
        Linux*)
            for p in /dev/ttyACM* /dev/ttyUSB*; do
                [ -e "$p" ] && echo "$p"
            done
            ;;
        *)
            for p in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/ttyACM* /dev/ttyUSB*; do
                [ -e "$p" ] && echo "$p"
            done
            ;;
    esac
    return 0
}

# Configure tty: 8N1, raw, no flow control, no echo. macOS uses -f, Linux -F.
configure_port() {
    local port="$1" baud="$2"
    if [ ! -e "$port" ]; then
        log_error "Port not found: $port"
        return 1
    fi
    case "$(uname -s)" in
        Darwin*) stty -f "$port" "$baud" cs8 -cstopb -parenb -ixon raw -echo 2>/dev/null ;;
        *)       stty -F "$port" "$baud" cs8 -cstopb -parenb -ixon raw -echo 2>/dev/null ;;
    esac
}

show_board_menu() {
    echo -e "${BOLD}Select a board:${NC}" >&2
    echo "" >&2
    local i tag
    for i in "${!BOARDS[@]}"; do
        local board="${BOARDS[$i]}"
        tag=$(board_hint_tag "$board")
        if [ -n "$tag" ]; then
            printf "  %d) %-22s ${CYAN}[%s]${NC}\n" "$((i + 1))" "$board" "$tag" >&2
        else
            printf "  %d) %s\n" "$((i + 1))" "$board" >&2
        fi
    done
    echo "" >&2
    echo "  s) Skip — just open a port" >&2
    echo "  q) Quit" >&2
    echo "" >&2
}

# Returns the chosen board name on stdout, empty for Skip, or non-zero exit
# for Quit / invalid. log_* go to stderr so the captured stdout stays clean.
interactive_pick_board() {
    show_board_menu
    local selection
    read -r -p "Enter selection: " selection
    case "$selection" in
        q|Q|quit|exit)  return 1 ;;
        s|S|skip)       echo ""; return 0 ;;
        [0-9]|[0-9][0-9])
            local idx=$((selection - 1))
            if [ "$idx" -ge 0 ] && [ "$idx" -lt "${#BOARDS[@]}" ]; then
                echo "${BOARDS[$idx]}"
                return 0
            fi
            log_error "Invalid selection: $selection"
            return 1
            ;;
        *)
            if is_valid_board "$selection"; then
                echo "$selection"
                return 0
            fi
            log_error "Invalid selection: $selection"
            return 1
            ;;
    esac
}

# Pick a port. If exactly one candidate, auto-pick. Otherwise prompt.
interactive_pick_port() {
    local ports
    ports=$(list_ports)

    if [ -z "$ports" ]; then
        log_error "No serial ports found."
        log_warn "Plug in the board (USB cable from ST-Link/VCP). On macOS, expect /dev/cu.usbmodem*"
        log_warn "If the board is plugged in but not showing up, the ST-Link VCP driver may need updating."
        return 1
    fi

    local -a port_list=()
    while IFS= read -r p; do
        port_list+=("$p")
    done <<<"$ports"

    if [ "${#port_list[@]}" -eq 1 ]; then
        log_info "Only one port found: ${port_list[0]} — opening."
        echo "${port_list[0]}"
        return 0
    fi

    echo -e "${BOLD}Select a serial port:${NC}" >&2
    echo "" >&2
    local i=1
    for p in "${port_list[@]}"; do
        printf "  %d) %s\n" "$i" "$p" >&2
        i=$((i + 1))
    done
    echo "" >&2
    echo "  q) Quit" >&2
    echo "" >&2

    local selection
    read -r -p "Enter selection: " selection
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

open_tio() {
    local port="$1" baud="$2"

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

    log_info "Opening $port @ $baud using tio"
    log_info "Detach: Ctrl-T q   |   List shortcuts: Ctrl-T ?"
    echo "" >&2

    tio -b "$baud" "$port"
}

# --read MODE: capture for N seconds, print transcript to stdout, exit.
# Bypasses tio (cat + bounded sleep is more reliable for one-shot capture).
read_mode() {
    local port="$1" baud="$2" duration="$3"
    [ -e "$port" ] || { log_error "Port not found: $port"; return 1; }
    configure_port "$port" "$baud" || return 1

    log_info "Capturing ${duration}s from $port @ $baud..."

    # Use a tmpfile + sized check so we can branch the exit code on
    # "got nothing back" — Claude can treat exit 1 as a dead board.
    local tmpfile
    tmpfile=$(mktemp)

    # `timeout(1)` is not in stock macOS, so manage the deadline manually.
    cat "$port" >"$tmpfile" 2>/dev/null &
    local reader_pid=$!
    sleep "$duration"
    kill "$reader_pid" 2>/dev/null || true
    wait "$reader_pid" 2>/dev/null || true

    cat "$tmpfile"
    local size
    size=$(wc -c <"$tmpfile" | tr -d ' ')
    rm -f "$tmpfile"

    if [ "$size" -eq 0 ]; then
        log_warn "No output captured in ${duration}s. Board silent? Already past banner?"
        return 1
    fi
    log_info "Captured ${size} bytes."
    return 0
}

# --exec MODE: send commands, settle, capture, exit.
# Like read_mode, this bypasses tio: piping commands through tio's stdin
# is brittle (Ctrl-T escape handling, terminal-mode quirks). Direct FD I/O
# against the device works on every tio version and on macOS-without-timeout(1).
exec_mode() {
    local port="$1" baud="$2" cmd_arg="$3" settle="$4"
    [ -e "$port" ] || { log_error "Port not found: $port"; return 1; }
    configure_port "$port" "$baud" || return 1

    log_info "Sending '$cmd_arg' to $port (settle=${settle}s)..."

    local tmpfile
    tmpfile=$(mktemp)

    # FD 3 is the bidirectional handle to the serial port.
    exec 3<>"$port"

    cat <&3 >"$tmpfile" 2>/dev/null &
    local reader_pid=$!

    # Cleanup if the user Ctrl-C's, or anywhere set -e fires.
    cleanup_exec() {
        kill "$reader_pid" 2>/dev/null || true
        wait "$reader_pid" 2>/dev/null || true
        exec 3<&- 2>/dev/null || true
        exec 3>&- 2>/dev/null || true
    }
    trap cleanup_exec EXIT INT TERM

    # Let any pending boot output flush in before injecting commands.
    sleep 0.3

    # CR+LF for safety — feb_console is line-based and accepts either.
    local c
    IFS=',' read -ra cmds <<<"$cmd_arg"
    for c in "${cmds[@]}"; do
        c="${c#"${c%%[![:space:]]*}"}"
        c="${c%"${c##*[![:space:]]}"}"
        printf '%s\r\n' "$c" >&3
        sleep 0.1
    done

    sleep "$settle"

    cleanup_exec
    trap - EXIT INT TERM

    cat "$tmpfile"
    local size
    size=$(wc -c <"$tmpfile" | tr -d ' ')
    rm -f "$tmpfile"

    if [ "$size" -eq 0 ]; then
        log_warn "No output captured. Board silent? Try --settle higher or check the port."
        return 1
    fi
    log_info "Captured ${size} bytes."
    return 0
}

show_help() {
    cat <<EOF
FEB Firmware Serial Console Helper (tio wrapper)

Usage: ./scripts/serial.sh [options]

Options:
  -b, --board <BOARD>      Print expected boot output for BOARD before opening
  -p, --port <PATH>        Open a specific serial port directly (skips menu)
      --baud <N>           Override baud rate (default $DEFAULT_BAUD)
      --list               List candidate serial ports and exit
      --read <SECONDS>     Capture serial output for N seconds, print, exit
      --exec <CMDS>        Comma-separated commands; send each, capture, exit
      --settle <SECONDS>   Settle time after --exec sends (default $DEFAULT_SETTLE)
  -h, --help               Show this help message

Examples:
  ./scripts/serial.sh                                  # Board menu → port menu
  ./scripts/serial.sh -b BMS                           # BMS hints + port menu
  ./scripts/serial.sh -p /dev/cu.usbmodem1234          # Open a specific port
  ./scripts/serial.sh -b PCU --read 3                  # Capture PCU's boot trace
  ./scripts/serial.sh -b BMS --exec "version,uptime"   # Round-trip BMS console
  ./scripts/serial.sh --baud 9600 -b DART              # Override baud
  ./scripts/serial.sh --list                           # List candidate ports

Boards with boot expectations on file:
  ${BOARDS[*]}

Note:
  After flashing, use this script (or tio directly) to verify the board is
  actually alive. 'Flash succeeded' is not proof the firmware works —
  \`version\` / \`uptime\` / \`help\` over serial is.

  --read and --exec print the captured transcript to STDOUT and diagnostics
  to STDERR, so:
      transcript=\$(./scripts/serial.sh -p /dev/cu.usbmodem1234 --exec "version")
  captures only what came back from the board. Exit 0 on bytes received,
  exit 1 on silence — branch on it to detect a dead board.

Prerequisites:
  - tio  (brew install tio  /  apt install tio)
EOF
}

main() {
    local board=""
    local port=""
    local baud="$DEFAULT_BAUD"
    local list_only=false
    local read_duration=""
    local exec_cmds=""
    local settle="$DEFAULT_SETTLE"

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -b|--board)    board="$2"; shift 2 ;;
            -p|--port)     port="$2";  shift 2 ;;
            --baud)        baud="$2";  shift 2 ;;
            --list)        list_only=true; shift ;;
            --read)        read_duration="$2"; shift 2 ;;
            --exec)        exec_cmds="$2";     shift 2 ;;
            --settle)      settle="$2";        shift 2 ;;
            -h|--help)     show_help; exit 0 ;;
            *)
                log_error "Unknown option: $1"
                echo "" >&2
                show_help >&2
                exit 1
                ;;
        esac
    done

    # --list works without tio installed — it's just an ls helper.
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

    if ! check_prerequisites; then
        show_setup_instructions
        exit 1
    fi

    if [ -n "$read_duration" ] && [ -n "$exec_cmds" ]; then
        log_error "--read and --exec are mutually exclusive."
        exit 1
    fi

    if [ -n "$board" ] && ! is_valid_board "$board" && [ "$board" != "Sensor_Nodes" ]; then
        log_warn "Unknown board: $board (continuing anyway — no expectations on file)"
    fi

    # Non-interactive modes never block on stdin: if no -p, auto-pick the
    # only available port, else fail loudly so Claude can branch on it.
    if [ -n "$read_duration" ] || [ -n "$exec_cmds" ]; then
        if [ -z "$port" ]; then
            local ports count
            ports=$(list_ports)
            count=$(echo "$ports" | grep -c . || true)
            if [ "$count" -eq 1 ]; then
                port=$(echo "$ports" | head -1)
                log_info "Auto-picked the only available port: $port"
            elif [ "$count" -eq 0 ]; then
                log_error "No serial ports found and no -p given."
                exit 1
            else
                log_error "Multiple serial ports — pass -p <PATH>:"
                echo "$ports" | sed 's/^/  /' >&2
                exit 1
            fi
        fi

        # Print expectations to stderr for human readers; stdout stays clean.
        if [ -n "$board" ]; then
            log_info "Board: $board"
            echo "" >&2
            expected_banner_for "$board" | sed 's/^/  /' >&2
            echo "" >&2
        fi

        if [ -n "$read_duration" ]; then
            read_mode "$port" "$baud" "$read_duration"
            exit $?
        else
            exec_mode "$port" "$baud" "$exec_cmds" "$settle"
            exit $?
        fi
    fi

    # Interactive flow from here on.
    log_header "FEB Serial Console"

    # If no -b, prompt for board (with a Skip option). If the user skipped,
    # `board` is empty — we just don't print expectations.
    if [ -z "$board" ]; then
        if ! board=$(interactive_pick_board); then
            exit 1
        fi
    fi

    if [ -n "$board" ]; then
        log_info "Board: $board"
        echo "" >&2
        expected_banner_for "$board" | sed 's/^/  /' >&2
        echo "" >&2
    fi

    if [ -z "$port" ]; then
        if ! port=$(interactive_pick_port); then
            exit 1
        fi
    fi

    open_tio "$port" "$baud"
}

main "$@"
