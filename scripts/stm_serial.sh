#!/usr/bin/env bash

set -euo pipefail

readonly DEFAULT_VENDOR_ID=1155
readonly DEFAULT_PRODUCT_ID=14164
readonly DEFAULT_BAUD=115200

usage() {
    cat <<'EOF'
Usage:
  stm_serial.sh path
  stm_serial.sh list
  stm_serial.sh monitor [baud]

Resolves the ST-Link virtual COM port on macOS from USB vendor/product data.

Environment:
  STM_SERIAL_PORT      Force an explicit serial device path.
  STM_ST_VENDOR_ID     Override the USB vendor ID filter. Default: 1155.
  STM_ST_PRODUCT_ID    Override the USB product ID filter. Default: 14164.
EOF
}

list_matching_ports() {
    local vendor_id product_id
    vendor_id="${STM_ST_VENDOR_ID:-$DEFAULT_VENDOR_ID}"
    product_id="${STM_ST_PRODUCT_ID:-$DEFAULT_PRODUCT_ID}"

    ioreg -r -c AppleUSBACMData -l -w 0 | awk -v vendor="$vendor_id" -v product="$product_id" '
        function flush() {
            if (!in_block) {
                return
            }

            if (vid == vendor && pid == product && tty_base != "") {
                printf "/dev/cu.%s%s\n", tty_base, tty_suffix
            }
        }

        /^\+-o AppleUSBACMData/ {
            flush()
            in_block = 1
            vid = ""
            pid = ""
            tty_base = ""
            tty_suffix = ""
            next
        }

        in_block {
            if ($0 ~ /"idVendor" = /) {
                line = $0
                sub(/^.*"idVendor" = /, "", line)
                sub(/[^0-9].*$/, "", line)
                vid = line
            } else if ($0 ~ /"idProduct" = /) {
                line = $0
                sub(/^.*"idProduct" = /, "", line)
                sub(/[^0-9].*$/, "", line)
                pid = line
            } else if ($0 ~ /"IOTTYBaseName" = "/) {
                line = $0
                sub(/^.*"IOTTYBaseName" = "/, "", line)
                sub(/".*$/, "", line)
                tty_base = line
            } else if ($0 ~ /"IOTTYSuffix" = "/) {
                line = $0
                sub(/^.*"IOTTYSuffix" = "/, "", line)
                sub(/".*$/, "", line)
                tty_suffix = line
            }
        }

        END {
            flush()
        }
    ' | awk 'NF && !seen[$0]++'
}

resolve_port() {
    local forced_port ports count

    forced_port="${STM_SERIAL_PORT:-}"
    if [[ -n "$forced_port" ]]; then
        if [[ ! -e "$forced_port" ]]; then
            echo "Configured STM_SERIAL_PORT does not exist: $forced_port" >&2
            exit 1
        fi

        printf '%s\n' "$forced_port"
        return
    fi

    ports="$(list_matching_ports)"
    count="$(printf '%s\n' "$ports" | sed '/^$/d' | wc -l | tr -d ' ')"

    if [[ "$count" == "0" ]]; then
        echo "No ST-Link virtual COM port found." >&2
        echo "Expected USB vendor/product: ${STM_ST_VENDOR_ID:-$DEFAULT_VENDOR_ID}/${STM_ST_PRODUCT_ID:-$DEFAULT_PRODUCT_ID}" >&2
        exit 1
    fi

    if [[ "$count" != "1" ]]; then
        echo "Multiple ST-Link virtual COM ports matched:" >&2
        printf '%s\n' "$ports" >&2
        echo "Set STM_SERIAL_PORT to the exact device path you want." >&2
        exit 1
    fi

    printf '%s\n' "$ports"
}

main() {
    local command baud port

    command="${1:-path}"

    case "$command" in
        path)
            resolve_port
            ;;
        list)
            list_matching_ports
            ;;
        monitor)
            baud="${2:-$DEFAULT_BAUD}"
            port="$(resolve_port)"
            echo "Opening $port at ${baud} baud"
            echo "Detach with Ctrl-A, then K."
            exec screen "$port" "$baud"
            ;;
        -h|--help|help)
            usage
            ;;
        *)
            echo "Unknown command: $command" >&2
            usage >&2
            exit 1
            ;;
    esac
}

main "$@"
