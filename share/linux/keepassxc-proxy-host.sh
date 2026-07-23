#!/usr/bin/env bash
# Host-side keepassxc-proxy launcher for native messaging.
# Optional: run the proxy inside a distrobox/toolbox that provides build deps (e.g. Botan).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROXY="${KEEPASSXC_PROXY_BIN:-$ROOT/build/src/proxy/keepassxc-proxy}"
BOX="${KEEPASSXC_DISTROBOX:-keepassxc-build}"

if [[ ! -x "$PROXY" ]]; then
  # Common alternate build directory name
  if [[ -x "$ROOT/build-linux/src/proxy/keepassxc-proxy" ]]; then
    PROXY="$ROOT/build-linux/src/proxy/keepassxc-proxy"
  fi
fi

if [[ ! -x "$PROXY" ]]; then
  echo "keepassxc-proxy missing: set KEEPASSXC_PROXY_BIN or build under ./build" >&2
  exit 1
fi

# Prefer a direct host binary when it loads (no container needed).
if ldd "$PROXY" >/dev/null 2>&1; then
  if ! ldd "$PROXY" 2>/dev/null | grep -q 'not found'; then
    exec "$PROXY" "$@"
  fi
fi

# Preserve stdin/stdout for native messaging. Prefer podman exec -i over
# "distrobox enter" which can disrupt the stdio pipe.
if command -v podman >/dev/null 2>&1 && podman inspect "$BOX" >/dev/null 2>&1; then
  if ! podman inspect -f '{{.State.Running}}' "$BOX" 2>/dev/null | grep -qx true; then
    distrobox enter -n "$BOX" -- true >/dev/null 2>&1 || true
  fi
  exec podman exec -i -u "$(id -un)" -w "$HOME" \
    -e HOME="$HOME" \
    -e XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}" \
    -e DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-}" \
    "$BOX" "$PROXY" "$@"
fi

if command -v distrobox >/dev/null 2>&1; then
  exec distrobox enter -n "$BOX" -- "$PROXY" "$@"
fi

exec "$PROXY" "$@"
