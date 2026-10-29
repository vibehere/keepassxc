#!/bin/bash
# Reliable KeePassXC launcher for WSL / Linux desktop.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# Prefer installed package; fall back to build output.
if command -v keepassxc >/dev/null 2>&1; then
  BIN=$(command -v keepassxc)
elif [[ -x "$ROOT/share/linux/out/KeePassXC" ]]; then
  BIN="$ROOT/share/linux/out/KeePassXC"
else
  echo "keepassxc not found" >&2
  exit 1
fi

# WSLg: keep the real runtime dir (do not replace with /tmp/...)
if [[ -d /mnt/wslg/runtime-dir ]]; then
  export XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir
fi
export DISPLAY="${DISPLAY:-:0}"
# X11 is more reliable under WSLg than Qt Wayland
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export QT_QPA_PLATFORM_PLUGIN_PATH="${QT_QPA_PLATFORM_PLUGIN_PATH:-}"

# Clear stale single-instance locks only if no process is alive
if ! pgrep -x keepassxc >/dev/null 2>&1; then
  rm -f "${XDG_RUNTIME_DIR:-/tmp}/org.keepassxc.KeePassXC"* 2>/dev/null || true
  rm -rf "${XDG_RUNTIME_DIR:-/tmp}/app/org.keepassxc.KeePassXC" 2>/dev/null || true
  # clear leftover runtime locks
  rm -f /tmp/runtime-keepass/org.keepassxc.KeePassXC* 2>/dev/null || true
  rm -rf /tmp/runtime-keepass/app/org.keepassxc.KeePassXC 2>/dev/null || true
fi

exec "$BIN" "$@"
