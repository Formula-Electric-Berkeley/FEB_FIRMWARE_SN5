#!/bin/bash
#
# Version management script for FEB_FIRMWARE_SN5
# Creates and pushes git tags to trigger the Tagged Release workflow
#
# Usage:
#   ./scripts/version.sh              # Auto-increment patch (v1.0.0 → v1.0.1)
#   ./scripts/version.sh patch        # Same as above
#   ./scripts/version.sh minor        # Bump minor (v1.0.1 → v1.1.0)
#   ./scripts/version.sh major        # Bump major (v1.1.0 → v2.0.0)
#   ./scripts/version.sh 2.5.0        # Set explicit version (→ v2.5.0)

set -e

# Get repository root
REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

# Get the latest version tag, default to v0.0.0 if none exist
LATEST_TAG=$(git describe --tags --abbrev=0 --match "v*" 2>/dev/null || echo "v0.0.0")
echo "Current version: $LATEST_TAG"

# Parse version components (strip leading 'v')
VERSION="${LATEST_TAG#v}"
IFS='.' read -r MAJOR MINOR PATCH <<< "$VERSION"

# Default to patch bump
BUMP_TYPE="${1:-patch}"

# Calculate new version based on argument
if [[ "$BUMP_TYPE" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  # Explicit version provided (e.g., 2.5.0)
  NEW_VERSION="$BUMP_TYPE"
elif [[ "$BUMP_TYPE" == "major" ]]; then
  MAJOR=$((MAJOR + 1))
  MINOR=0
  PATCH=0
  NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}"
elif [[ "$BUMP_TYPE" == "minor" ]]; then
  MINOR=$((MINOR + 1))
  PATCH=0
  NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}"
elif [[ "$BUMP_TYPE" == "patch" ]]; then
  PATCH=$((PATCH + 1))
  NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}"
else
  echo "Error: Invalid argument '$BUMP_TYPE'"
  echo ""
  echo "Usage:"
  echo "  $0              # Auto-increment patch"
  echo "  $0 patch        # Increment patch version"
  echo "  $0 minor        # Increment minor version"
  echo "  $0 major        # Increment major version"
  echo "  $0 X.Y.Z        # Set explicit version"
  exit 1
fi

NEW_TAG="v${NEW_VERSION}"

# Check if tag already exists
if git rev-parse "$NEW_TAG" >/dev/null 2>&1; then
  echo "Error: Tag $NEW_TAG already exists"
  exit 1
fi

# TODO: Update FEB_Version.h when created
# This can be enabled once a version header is added via CubeMX workflow
# VERSION_HEADER="$REPO_ROOT/common/FEB_Version.h"
# if [[ -f "$VERSION_HEADER" ]]; then
#   sed -i '' "s/#define FEB_VERSION.*/#define FEB_VERSION \"${NEW_VERSION}\"/" "$VERSION_HEADER"
#   git add "$VERSION_HEADER"
#   git commit -m "Bump version to ${NEW_VERSION}"
# fi

echo "Creating tag: $NEW_TAG"
git tag "$NEW_TAG"

echo "Pushing tag to origin..."
git push origin "$NEW_TAG"

# Get remote URL to construct release link
REMOTE_URL=$(git remote get-url origin 2>/dev/null || echo "")
if [[ "$REMOTE_URL" == *"github.com"* ]]; then
  # Convert SSH URL to HTTPS if needed
  REPO_PATH=$(echo "$REMOTE_URL" | sed -E 's|.*github\.com[:/](.*)\.git$|\1|' | sed 's|\.git$||')
  echo ""
  echo "Release will be available at:"
  echo "  https://github.com/${REPO_PATH}/releases/tag/${NEW_TAG}"
fi

echo ""
echo "Done! Tagged $NEW_TAG (was $LATEST_TAG)"
