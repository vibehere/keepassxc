#!/usr/bin/env bash
# Copy the WebAuthn cross-platform bridge into a local Chromium extension dir.
# Chromium only honors --load-extension on a fresh start.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
EXT_SRC="${ROOT}/webauthn-cross-platform-bridge"
EXT_DST="${XDG_DATA_HOME:-$HOME/.local/share}/webauthn-cross-platform-bridge"

if [[ ! -f "${EXT_SRC}/manifest.json" ]]; then
  echo "missing extension at ${EXT_SRC}" >&2
  exit 1
fi

mkdir -p "${EXT_DST}"
cp -a "${EXT_SRC}/." "${EXT_DST}/"

echo "Extension ready at ${EXT_DST}"
echo "Chromium: chrome://extensions → Developer mode → Load unpacked → that folder"
echo "Or restart Chromium with: --load-extension=${EXT_DST}"
