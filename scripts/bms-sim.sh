#!/bin/bash
#
# BMS ECU simulator: build the host-side sim and run its scenarios.
#
# Compiles the unmodified BMS state machine for this machine (no board, no
# ARM toolchain) and drives it with a plant model. See BMS/sim/README.md.
#
# Usage:
#   ./scripts/bms-sim.sh                    # build + run every scenario
#   ./scripts/bms-sim.sh list               # list scenarios
#   ./scripts/bms-sim.sh run <scenario>     # one scenario, full log
#   ./scripts/bms-sim.sh run <scenario> --csv out.csv -v
#
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/bms_sim"

# Host compiler only: never inherit the firmware's ARM toolchain file.
env -u CMAKE_TOOLCHAIN_FILE cmake -S "$REPO_ROOT/BMS/sim" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" --parallel >/dev/null

if [[ $# -eq 0 ]]; then
    set -- all -q
fi
exec "$BUILD_DIR/bms_sim" "$@"
