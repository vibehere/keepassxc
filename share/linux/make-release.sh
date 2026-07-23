#!/bin/bash
set -euo pipefail

# Create an annotated tag and push it. GitHub Actions then builds and publishes the Release.
# Usage:
#   bash share/linux/make-release.sh
#   bash share/linux/make-release.sh v2.8.0-ospasskeys.1

TAG="${1:-}"
if [[ -z "$TAG" ]]; then
  DATE="$(date -u +%Y%m%d)"
  TAG="v2.8.0-ospasskeys.${DATE}"
fi

if [[ ! "$TAG" =~ ^v ]]; then
  TAG="v$TAG"
fi

git rev-parse --is-inside-work-tree >/dev/null
BRANCH="$(git branch --show-current)"
echo "branch=$BRANCH tag=$TAG"

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "tag already exists: $TAG"
  exit 1
fi

if [[ -n "$(git status --porcelain)" ]]; then
  echo "working tree not clean; commit or stash first"
  git status -sb
  exit 1
fi

git tag -a "$TAG" -m "Release $TAG"
git push origin "$BRANCH"
git push origin "$TAG"
echo "Pushed $TAG. Open Actions on GitHub to watch the release build."
