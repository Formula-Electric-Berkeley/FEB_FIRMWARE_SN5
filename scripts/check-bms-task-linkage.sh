#!/bin/bash
#
# Task-linkage guard for the BMS
#
# The CubeMX-generated freertos.c defines the FreeRTOS task entry points as
# __weak stubs; the real bodies live in BMS/Core/User/Src/*.cpp. In C++ a
# function defined without extern "C" is silently name-mangled, so the weak stub
# wins at link time: the build is green, the task exists, and it does nothing.
#
# Fails if any expected task entry point is missing or still a weak (W) symbol
# in the linked ELF.
#
# Usage:
#   ./scripts/check-bms-task-linkage.sh <BMS.elf> [nm-tool]
#
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -lt 1 ]]; then
    sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

ELF="$1"
NM="${2:-arm-none-eabi-nm}"

TASKS=(
    StartUartRxTask
    StartADBMSTask
    StartTPSTask
    StartBMSTaskRx
    StartBMSTaskTx
    StartSMTask
)

fail=0
for sym in "${TASKS[@]}"; do
    type="$("$NM" --defined-only "$ELF" | awk -v s="$sym" '$3 == s {print $2}')"
    if [[ -z "$type" ]]; then
        echo "check-bms-task-linkage: '$sym' is not defined in $ELF (name-mangled or removed?)" >&2
        fail=1
    elif [[ "$type" == "W" || "$type" == "w" ]]; then
        echo "check-bms-task-linkage: '$sym' is still the WEAK CubeMX stub - the real task body is not linked (missing extern \"C\"?)" >&2
        fail=1
    fi
done

if [[ $fail -ne 0 ]]; then
    exit 1
fi
echo "check-bms-task-linkage: OK (${#TASKS[@]} task entry points bound to real definitions)"
