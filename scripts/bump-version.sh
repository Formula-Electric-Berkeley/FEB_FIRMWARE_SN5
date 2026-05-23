#!/bin/bash
#
# bump-version.sh - unified firmware version bump + commit + annotated tag + push
#
# Per-board tags have the shape  <BOARD>-v<MAJOR>.<MINOR>.<PATCH>
# Repo-wide tags have the shape  v<MAJOR>.<MINOR>.<PATCH>
#
# Usage:
#   ./scripts/bump-version.sh                           # auto-detect from staged/unstaged diff
#   ./scripts/bump-version.sh BMS                       # bump BMS patch
#   ./scripts/bump-version.sh BMS minor                 # bump BMS minor
#   ./scripts/bump-version.sh BMS 2.5.0                 # set BMS to explicit version
#   ./scripts/bump-version.sh repo                      # bump repo-wide patch
#   ./scripts/bump-version.sh all                       # bump every board + repo patch
#   ./scripts/bump-version.sh common                    # bump common/ patch (no board tags)
#
# Flags (any position):
#   --no-push       Create commit + tag locally, skip git push
#   --allow-dirty   Allow bumping on a dirty tree (unstaged or other-target changes)
#   --yes           Skip the confirmation prompt in auto-detect mode
#   --help          Show this help
#
# Invocable identically from .vscode/tasks.json and the shell - the VSCode
# task wraps this script with the same positional arguments. Any other
# entry point (release CI, helper slash commands) MUST reuse this script
# so the bump-commit-tag-push flow stays consistent.
#

set -euo pipefail

BOARDS=(BMS DASH DART DCU LVPDB PCU Sensor_Nodes UART UART_TEST)

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [ -z "$REPO_ROOT" ]; then
    echo "Error: not inside a git repository." >&2
    exit 1
fi
cd "$REPO_ROOT"

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; BLUE=''; BOLD=''; NC=''
fi
log_info()  { echo -e "${GREEN}[bump]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[bump]${NC} $*" >&2; }
log_error() { echo -e "${RED}[bump]${NC} $*" >&2; }

show_help() {
    sed -n '3,28p' "$0" | sed 's/^# \{0,1\}//'
}

# ---------------------------------------------------------------------------
# Flag parsing (positional + --flags in any order)
# ---------------------------------------------------------------------------
PUSH=1
ALLOW_DIRTY=0
YES=0
POS=()
for arg in "$@"; do
    case "$arg" in
        --no-push)      PUSH=0 ;;
        --allow-dirty)  ALLOW_DIRTY=1 ;;
        --yes|-y)       YES=1 ;;
        --help|-h)      show_help; exit 0 ;;
        -*)             log_error "unknown flag: $arg"; exit 1 ;;
        *)              POS+=("$arg") ;;
    esac
done

# ---------------------------------------------------------------------------
# Version parsing / compute
# ---------------------------------------------------------------------------
is_board() {
    local name="$1"
    for b in "${BOARDS[@]}"; do
        [ "$b" = "$name" ] && return 0
    done
    return 1
}

read_version() {
    local file="$1"
    if [ -f "$file" ]; then
        head -n1 "$file" | tr -d '[:space:]'
    else
        echo "0.0.0"
    fi
}

parse_version() {
    # Set MAJOR/MINOR/PATCH globals from "X.Y.Z"
    local v="$1"
    if ! [[ "$v" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
        log_error "malformed version: '$v'"
        exit 1
    fi
    MAJOR="${BASH_REMATCH[1]}"
    MINOR="${BASH_REMATCH[2]}"
    PATCH="${BASH_REMATCH[3]}"
}

compute_new_version() {
    # $1=current "X.Y.Z", $2=bump type ("patch"|"minor"|"major"|"X.Y.Z"|"")
    # Prints new version.
    local current="$1"
    local bump="${2:-patch}"
    if [[ "$bump" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "$bump"
        return
    fi
    parse_version "$current"
    case "$bump" in
        patch) PATCH=$((PATCH + 1)) ;;
        minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
        major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
        *) log_error "invalid bump: '$bump' (patch|minor|major|X.Y.Z)"; exit 1 ;;
    esac
    echo "${MAJOR}.${MINOR}.${PATCH}"
}

# ---------------------------------------------------------------------------
# Git pre-flight
# ---------------------------------------------------------------------------
dirty_tree() {
    # "dirty" here means: staged or unstaged changes exist anywhere EXCEPT
    # in the VERSION files we're about to modify. We allow co-located
    # changes on --allow-dirty.
    ! git diff-index --quiet HEAD -- 2>/dev/null
}

branch_has_upstream() {
    git rev-parse --abbrev-ref '@{u}' >/dev/null 2>&1
}

ensure_clean_or_allowed() {
    if dirty_tree && [ "$ALLOW_DIRTY" -ne 1 ]; then
        log_error "working tree has uncommitted changes."
        log_error "commit or stash them first, or re-run with --allow-dirty."
        log_error "(the bumped VERSION files would be mixed with unrelated changes otherwise)"
        git status --short >&2
        exit 1
    fi
    if [ "$PUSH" -eq 1 ] && ! branch_has_upstream; then
        log_warn "current branch has no upstream; will tag locally but skip push."
        PUSH=0
    fi
}

# ---------------------------------------------------------------------------
# Auto-detect mode: what did we touch?
# ---------------------------------------------------------------------------
detect_dirty_targets() {
    # Prints space-separated target tokens (board names, "common", "repo")
    # based on which paths have staged or unstaged changes.
    local changed
    changed=$( { git diff --cached --name-only; git diff --name-only; } | sort -u )
    local hits=()
    local common_hit=0
    for b in "${BOARDS[@]}"; do
        if echo "$changed" | grep -qE "^${b}/"; then
            hits+=("$b")
        fi
    done
    if echo "$changed" | grep -qE "^common/"; then
        common_hit=1
    fi
    if [ "$common_hit" -eq 1 ]; then
        # Shared library changes affect every board.
        echo "all"
        return
    fi
    if [ "${#hits[@]}" -eq 0 ]; then
        echo ""
        return
    fi
    echo "${hits[@]}"
}

# ---------------------------------------------------------------------------
# Core bump - edit VERSION, stage it, return the new version via stdout
# ---------------------------------------------------------------------------
bump_version_file() {
    # $1 = path to VERSION file, $2 = bump type. Prints new version.
    local file="$1"
    local bump="$2"
    local current new
    current=$(read_version "$file")
    new=$(compute_new_version "$current" "$bump")
    mkdir -p "$(dirname "$file")"
    echo "$new" > "$file"
    git add "$file"
    echo "$new"
}

# ---------------------------------------------------------------------------
# Tag / commit helpers
# ---------------------------------------------------------------------------
ensure_tag_free() {
    local tag="$1"
    if git rev-parse "$tag" >/dev/null 2>&1; then
        log_error "tag already exists: $tag"
        exit 1
    fi
}

commit_and_tag() {
    # $1 = commit message
    # $2... = tags to create (annotated). Each tag gets the same message body.
    local message="$1"; shift
    git commit -m "$message"
    for tag in "$@"; do
        git tag -a "$tag" -m "$message"
    done
    log_info "Committed: $message"
    for tag in "$@"; do
        log_info "  tag: $tag"
    done
    if [ "$PUSH" -eq 1 ]; then
        git push
        for tag in "$@"; do
            git push origin "$tag"
        done
        log_info "Pushed commit + tags to origin."
    else
        log_warn "--no-push: tags created locally only. Push manually when ready:"
        for tag in "$@"; do
            echo "    git push origin $tag"
        done
    fi
}

# ---------------------------------------------------------------------------
# Bump flavours
# ---------------------------------------------------------------------------
bump_one_board() {
    local board="$1"; local bump="${2:-patch}"
    local version_file="$REPO_ROOT/$board/VERSION"
    local new
    new=$(bump_version_file "$version_file" "$bump")
    local tag="${board}-v${new}"
    ensure_tag_free "$tag"
    commit_and_tag "${board}: bump to ${new}" "$tag"
}

bump_repo() {
    local bump="${1:-patch}"
    local version_file="$REPO_ROOT/VERSION"
    local new
    new=$(bump_version_file "$version_file" "$bump")
    local tag="v${new}"
    ensure_tag_free "$tag"
    commit_and_tag "repo: bump to ${new}" "$tag"
}

bump_common() {
    local bump="${1:-patch}"
    local version_file="$REPO_ROOT/common/VERSION"
    local new
    new=$(bump_version_file "$version_file" "$bump")
    # No standalone tag for common - it always rides along with "all".
    commit_and_tag "common: bump to ${new}"
}

bump_all() {
    # $1 = bump type. Bumps every board VERSION + common VERSION + repo
    # VERSION in a single commit with one annotated tag per affected
    # target. This is what "common changed" or a coordinated release
    # should use.
    local bump="${1:-patch}"
    local tags=()
    local summary=""

    # Stage all VERSION updates before committing so it's one atomic change.
    local board_new common_new repo_new tag
    for b in "${BOARDS[@]}"; do
        board_new=$(bump_version_file "$REPO_ROOT/$b/VERSION" "$bump")
        tag="${b}-v${board_new}"
        ensure_tag_free "$tag"
        tags+=("$tag")
        summary="${summary}${b}=${board_new}, "
    done
    common_new=$(bump_version_file "$REPO_ROOT/common/VERSION" "$bump")
    summary="${summary}common=${common_new}, "

    repo_new=$(bump_version_file "$REPO_ROOT/VERSION" "$bump")
    tag="v${repo_new}"
    ensure_tag_free "$tag"
    tags+=("$tag")
    summary="${summary}repo=${repo_new}"

    commit_and_tag "all: bump (${summary})" "${tags[@]}"
}

# ---------------------------------------------------------------------------
# Auto-detect dispatch
# ---------------------------------------------------------------------------
run_auto_detect() {
    # $1 = bump type ("" defaults to patch)
    local bump="${1:-patch}"
    local targets
    targets=$(detect_dirty_targets)

    if [ -z "$targets" ]; then
        log_error "no board or common/ changes detected in the working tree."
        log_error "stage or modify files first, or pass an explicit target:"
        log_error "  $(basename "$0") BMS patch"
        log_error "  $(basename "$0") repo patch"
        exit 1
    fi

    echo -e "${BOLD}Auto-detected targets:${NC} $targets"
    echo "Bump type: $bump"
    if [ "$YES" -ne 1 ]; then
        read -r -p "Proceed with the bump? [y/N] " reply
        case "$reply" in
            [yY]|[yY][eE][sS]) ;;
            *) log_warn "aborted by user."; exit 0 ;;
        esac
    fi

    if [ "$targets" = "all" ]; then
        bump_all "$bump"
    else
        for b in $targets; do
            bump_one_board "$b" "$bump"
        done
    fi
}

# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------
main() {
    ensure_clean_or_allowed

    if [ "${#POS[@]}" -eq 0 ]; then
        run_auto_detect "patch"
        return
    fi

    local first="${POS[0]}"
    local rest=""
    if [ "${#POS[@]}" -ge 2 ]; then
        rest="${POS[1]}"
    fi

    case "$first" in
        repo)    bump_repo   "${rest:-patch}" ;;
        all)     bump_all    "${rest:-patch}" ;;
        common)  bump_common "${rest:-patch}" ;;
        *)
            if is_board "$first"; then
                bump_one_board "$first" "${rest:-patch}"
            else
                log_error "unknown target: '$first'"
                log_error "expected a board name, 'repo', 'common', or 'all'."
                exit 1
            fi
            ;;
    esac
}

main
