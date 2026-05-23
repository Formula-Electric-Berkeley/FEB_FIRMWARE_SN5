#!/bin/bash
#
# auto-bump-merge.sh — run from CI after a PR merges to main. Bumps the
# minor version (patch resets to 0) for every board VERSION whose tree was
# touched by the PR, plus common/VERSION (if common/ changed) and the
# repo-wide VERSION, then commits and tags. The `git push` happens in the
# workflow step (this script does not push).
#
# Usage:
#   ./scripts/auto-bump-merge.sh <BASE_SHA> <MERGE_SHA> [<PR_NUMBER>]
#
# Detection rules mirror auto-bump-commit.sh and scripts/bump-version.sh:
#   - any path under <BOARD>/ in the PR diff flags that board's VERSION
#   - any path under common/ flags every board + common
#   - repo VERSION always bumps alongside
#
# Tags follow the existing release convention: <BOARD>-v<X.Y.Z> and v<X.Y.Z>.
# No tag for common (matches bump-version.sh::bump_common).
#
# Commit message carries a `[skip ci]` marker so CI jobs that also run on
# pushes to main (build.yml, quality.yml, etc.) don't fire a second time.
#

set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <BASE_SHA> <MERGE_SHA> [PR_NUMBER]" >&2
    exit 1
fi

BASE_SHA="$1"
MERGE_SHA="$2"
PR_NUMBER="${3:-}"

BOARDS=(BMS DASH DART DCU LVPDB PCU Sensor_Nodes UART UART_TEST)

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

read_version() {
    local file="$1"
    if [ -f "$file" ]; then
        head -n1 "$file" | tr -d '[:space:]'
    else
        echo "0.0.0"
    fi
}

bump_minor() {
    local v="$1"
    if ! [[ "$v" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
        echo "auto-bump-merge: malformed version '$v'" >&2
        exit 1
    fi
    echo "${BASH_REMATCH[1]}.$((BASH_REMATCH[2] + 1)).0"
}

CHANGED="$(git diff --name-only "$BASE_SHA" "$MERGE_SHA")"
if [ -z "$CHANGED" ]; then
    echo "auto-bump-merge: no files changed between $BASE_SHA and $MERGE_SHA; nothing to do."
    exit 0
fi

TARGETS=()
if echo "$CHANGED" | grep -qE '^common/'; then
    TARGETS=("${BOARDS[@]}" common)
else
    for b in "${BOARDS[@]}"; do
        if echo "$CHANGED" | grep -qE "^${b}/"; then
            TARGETS+=("$b")
        fi
    done
fi

if [ "${#TARGETS[@]}" -eq 0 ]; then
    echo "auto-bump-merge: no board or common/ changes in this PR; skipping bump."
    exit 0
fi

TARGETS+=(repo)

declare -a TAGS=()
declare -a SUMMARY_PARTS=()

bump_target() {
    local target="$1"
    local file
    case "$target" in
        repo)   file="VERSION" ;;
        common) file="common/VERSION" ;;
        *)      file="$target/VERSION" ;;
    esac

    local current new
    current="$(read_version "$file")"
    new="$(bump_minor "$current")"
    mkdir -p "$(dirname "$file")"
    echo "$new" > "$file"
    git add "$file"
    SUMMARY_PARTS+=("${target}=${new}")

    case "$target" in
        repo)   TAGS+=("v${new}") ;;
        common) ;;  # matches bump-version.sh::bump_common — no standalone tag
        *)      TAGS+=("${target}-v${new}") ;;
    esac

    echo "  $file: $current -> $new"
}

for t in "${TARGETS[@]}"; do
    bump_target "$t"
done

# Guard: abort if any tag we want to create already exists. Prevents the
# workflow from pushing conflicting tags if someone hand-tagged the merge.
for tag in "${TAGS[@]}"; do
    if git rev-parse "$tag" >/dev/null 2>&1; then
        echo "auto-bump-merge: tag $tag already exists; aborting." >&2
        exit 1
    fi
done

PR_SUFFIX=""
if [ -n "$PR_NUMBER" ]; then
    PR_SUFFIX=" after PR #${PR_NUMBER}"
fi
SUMMARY_JOINED="$(IFS=, ; echo "${SUMMARY_PARTS[*]}")"
MESSAGE="auto: bump minor${PR_SUFFIX} (${SUMMARY_JOINED}) [skip ci]"

git commit -m "$MESSAGE"
for tag in "${TAGS[@]}"; do
    git tag -a "$tag" -m "$MESSAGE"
done

echo "auto-bump-merge: committed '$MESSAGE'"
echo "auto-bump-merge: tags created: ${TAGS[*]}"
