#!/bin/bash
# Launch a KeePassXC binary via distrobox/toolbox when host libs are missing.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${KEEPASSXC_BIN:-$ROOT/build/src/keepassxc}"
BOX="${KEEPASSXC_DISTROBOX:-keepassxc-build}"

if [[ ! -x "$BIN" && -x "$ROOT/build-linux/src/keepassxc" ]]; then
  BIN="$ROOT/build-linux/src/keepassxc"
fi

if [[ ! -x "$BIN" ]]; then
  echo "missing binary: set KEEPASSXC_BIN or build under ./build" >&2
  exit 1
fi

if ! command -v distrobox >/dev/null 2>&1; then
  echo "distrobox not found" >&2
  exit 1
fi

exec distrobox enter -n "$BOX" -- "$BIN" "$@"
