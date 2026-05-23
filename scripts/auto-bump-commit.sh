#!/bin/bash
#
# auto-bump-commit.sh — pre-commit hook that increments the patch version on
# every commit for each board/common directory touched plus the repo-wide
# VERSION file. Registered in .pre-commit-config.yaml (stages: [pre-commit]).
#
# Detection rules match scripts/bump-version.sh's auto-detect mode:
#   - any staged path under <BOARD>/ flags that board's VERSION
#   - any staged path under common/ flags every board + common VERSION
#   - repo-wide VERSION always bumps when at least one board/common bumps
#
# If the commit already stages a VERSION file (user ran /version manually
# or bump-version.sh), the hook no-ops to avoid double-bumping.
#
# The hook writes updated VERSION files and `git add`s them so they land in
# the in-progress commit. No new commit, no tag, no push — those belong to
# explicit release flows (bump-version.sh / auto-bump-merge.sh).
#

set -euo pipefail

BOARDS=(BMS DASH DART DCU LVPDB PCU Sensor_Nodes UART UART_TEST)

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [ -z "$REPO_ROOT" ]; then
    echo "auto-bump: not inside a git repository, skipping." >&2
    exit 0
fi
cd "$REPO_ROOT"

STAGED="$(git diff --cached --name-only)"
if [ -z "$STAGED" ]; then
    exit 0
fi

# Respect manual version bumps — if the user is committing a VERSION change,
# assume they meant the exact version they staged.
if echo "$STAGED" | grep -qE '(^|/)VERSION$'; then
    echo "auto-bump: VERSION already staged, skipping automatic bump." >&2
    exit 0
fi

read_version() {
    local file="$1"
    if [ -f "$file" ]; then
        head -n1 "$file" | tr -d '[:space:]'
    else
        echo "0.0.0"
    fi
}

bump_patch() {
    local v="$1"
    if ! [[ "$v" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
        echo "auto-bump: malformed version '$v'" >&2
        exit 1
    fi
    echo "${BASH_REMATCH[1]}.${BASH_REMATCH[2]}.$((BASH_REMATCH[3] + 1))"
}

# Detect touched targets.
TARGETS=()
if echo "$STAGED" | grep -qE '^common/'; then
    # Common library changes ripple to every board, matching bump-version.sh.
    TARGETS=("${BOARDS[@]}" common)
else
    for b in "${BOARDS[@]}"; do
        if echo "$STAGED" | grep -qE "^${b}/"; then
            TARGETS+=("$b")
        fi
    done
fi

if [ "${#TARGETS[@]}" -eq 0 ]; then
    # Commit doesn't touch any board or common — leave VERSION files alone.
    exit 0
fi

# Always bump the repo VERSION alongside board/common bumps so the "repo"
# column in the firmware version command tracks overall churn.
TARGETS+=(repo)

bump_file() {
    local file="$1"
    local current new
    current="$(read_version "$file")"
    new="$(bump_patch "$current")"
    mkdir -p "$(dirname "$file")"
    echo "$new" > "$file"
    git add "$file"
    echo "  $file: $current -> $new"
}

echo "auto-bump: patch bump for: ${TARGETS[*]}" >&2
for t in "${TARGETS[@]}"; do
    case "$t" in
        repo)   bump_file "VERSION" ;;
        common) bump_file "common/VERSION" ;;
        *)      bump_file "$t/VERSION" ;;
    esac
done >&2

exit 0
