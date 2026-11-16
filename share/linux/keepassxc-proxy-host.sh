#!/usr/bin/env bash
# Host-side keepassxc-proxy launcher for native messaging.
# Runs the proxy inside the distrobox that built KeePassXC when host libs are missing.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROXY="${KEEPASSXC_PROXY_BIN:-}"
BOX="${KEEPASSXC_DISTROBOX:-keepassxc-build}"

if [[ -z "$PROXY" ]]; then
  for candidate in \
    "$ROOT/build/src/proxy/keepassxc-proxy" \
    "$ROOT/build-linux/src/proxy/keepassxc-proxy"
  do
    if [[ -x "$candidate" ]]; then
      PROXY="$candidate"
      break
    fi
  done
fi

if [[ -z "${PROXY:-}" || ! -x "$PROXY" ]]; then
  echo "keepassxc-proxy missing: set KEEPASSXC_PROXY_BIN or build under ./build" >&2
  exit 1
fi

host_proxy_ok() {
  command -v ldd >/dev/null 2>&1 || return 1
  ldd "$PROXY" >/dev/null 2>&1 || return 1
  ! ldd "$PROXY" 2>/dev/null | grep -q 'not found'
}

# Prefer a direct host binary when its shared libs resolve.
if host_proxy_ok; then
  exec "$PROXY" "$@"
fi

# Preserve stdin/stdout for native messaging. Prefer podman exec -i over
# "distrobox enter" which can disrupt the stdio pipe / prompt interactively.
if command -v podman >/dev/null 2>&1 && podman inspect "$BOX" >/dev/null 2>&1; then
  if ! podman inspect -f '{{.State.Running}}' "$BOX" 2>/dev/null | grep -qx true; then
    # Non-interactive wake-up; ignore failures and let exec report errors.
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

echo "keepassxc-proxy needs distrobox '$BOX' (host missing Botan/Qt deps)" >&2
exit 1
