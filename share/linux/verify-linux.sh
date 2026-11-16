#!/bin/bash
# Smoke-verify Linux OS passkeys + packaging prerequisites.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT=${OUT:-$ROOT/share/linux/out}
REPORT=$OUT/verify-report.txt
mkdir -p "$OUT"
{
  echo "=== KeePassXC Linux verify $(date -Iseconds) ==="
  echo "kernel: $(uname -r)"
  echo

  echo "-- binaries --"
  for b in KeePassXC keepassxc-proxy keepassxc-cli; do
    if [[ -x $OUT/$b ]]; then
      echo "OK $b ($(stat -c%s "$OUT/$b") bytes)"
      "$OUT/$b" --version 2>&1 | head -2 || true
    else
      echo "MISSING $b"
    fi
  done
  echo

  echo "-- uhid / OS passkeys --"
  if [[ -e /dev/uhid ]]; then
    echo "UHID_NODE=present"
    if [[ -w /dev/uhid ]]; then
      echo "UHID_WRITE=yes"
    else
      echo "UHID_WRITE=no (need udev/group)"
    fi
  else
    echo "UHID_NODE=missing (expected on WSL — use native Linux/VM for OS passkeys)"
  fi
  if command -v modprobe >/dev/null; then
    sudo modprobe uhid 2>&1 | head -5 || echo "modprobe uhid failed (normal on WSL)"
  fi
  [[ -e /dev/uhid ]] && echo "UHID_AFTER_MODPROBE=present" || echo "UHID_AFTER_MODPROBE=still_missing"
  echo

  echo "-- deb package --"
  deb=$(ls -1 "$OUT"/keepassxc-ospasskeys_*.deb 2>/dev/null | head -1 || true)
  if [[ -n ${deb:-} ]]; then
    echo "OK $deb"
    dpkg-deb -I "$deb" | sed -n '1,20p'
  else
    echo "MISSING deb (run pack-deb.sh)"
  fi
  echo

  echo "-- browser addon path --"
  echo "BROWSER feature expected ON; use KeePassXC-Browser + keepassxc-proxy for fill/TOTP/passkeys."
  echo "OS passkeys path is additive and opt-in via the OS Passkeys toolbar toggle."
  echo
  echo "VERIFY_DONE"
} | tee "$REPORT"
