#!/bin/bash
#
# Bench-flag guard for the BMS
#
# Fails if any BMS bench-mode safety override is committed ENABLED in source.
# The in-source defaults in BMS/Core/User/Inc/FEB_Const.h MUST stay 0 so a
# real-pack / scrutineering build can never ship with ADBMS safety checks
# disabled. Bench bring-up enables them via the build flag instead:
#
#     cmake --preset Debug -DBMS_BENCH_MODE=ON
#
# Usage:
#   ./scripts/check-bench-flags.sh   # exits 1 if any override defaults to non-zero
#   ./scripts/check-bench-flags.sh -h
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR/.."
HEADER="$REPO_ROOT/BMS/Core/User/Inc/FEB_Const.h"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

# The four compile-time overrides that suppress ADBMS safety enforcement.
FLAGS=(
    FEB_BMS_DISABLE_ADBMS_CHECKS
    FEB_BMS_DISABLE_TEMP_CHECKS
    FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS
    FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS
)

if [[ ! -f "$HEADER" ]]; then
    echo "check-bench-flags: header not found: $HEADER" >&2
    exit 1
fi

fail=0
for flag in "${FLAGS[@]}"; do
    # The in-source DEFAULT is the '#define <flag> N' immediately following the
    # '#ifndef <flag>' guard. The '#define <flag> 1' lines under
    # '#if FEB_BMS_DISABLE_ADBMS_CHECKS' (each preceded by '#undef') are the
    # documented master-override implication, NOT a default — the #ifndef anchor
    # below skips them.
    val=$(awk -v f="$flag" '
        $0 ~ "^#ifndef " f "$" { armed = 1; next }
        armed && $0 ~ "^#define " f " " { print $3; exit }
        armed { armed = 0 }
    ' "$HEADER")

    if [[ -z "$val" ]]; then
        echo "check-bench-flags: no default #define found for $flag (guard expects an '#ifndef $flag' block)" >&2
        fail=1
    elif [[ "$val" != "0" ]]; then
        echo "check-bench-flags: $flag default is '$val' (must be 0) in BMS/Core/User/Inc/FEB_Const.h" >&2
        echo "                   enable bench mode via the build flag instead: cmake -DBMS_BENCH_MODE=ON" >&2
        fail=1
    fi
done

if [[ "$fail" -ne 0 ]]; then
    echo "check-bench-flags: FAILED — a bench-mode safety override is enabled in source." >&2
    exit 1
fi

echo "check-bench-flags: OK — all BMS bench overrides default to 0."
