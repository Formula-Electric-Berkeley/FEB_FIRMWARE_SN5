#!/bin/bash
#
# Backwards-compatible repo-wide version bumper.
#
# Kept as a thin wrapper so existing callers (GitHub Actions, the /version
# slash command, external scripts) keep working unchanged. All real logic
# now lives in bump-version.sh, which also handles per-board tags, the
# auto-detect flow, and the "common/ changed -> bump everything" case.
#
# Usage:
#   ./scripts/version.sh              # bump repo patch
#   ./scripts/version.sh patch        # same
#   ./scripts/version.sh minor        # bump repo minor
#   ./scripts/version.sh major        # bump repo major
#   ./scripts/version.sh 2.5.0        # set explicit repo version
#
# For per-board bumps use bump-version.sh directly:
#   ./scripts/bump-version.sh BMS patch
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/bump-version.sh" repo "$@"
