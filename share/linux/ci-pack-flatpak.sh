#!/bin/bash
# Build a single-file Flatpak bundle (Bazzite / Fedora Atomic / most desktops).
set -euo pipefail

OUT_DIR=dist
VERSION=2.8.0
ARCH="$(uname -m)"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MANIFEST="$ROOT/share/flatpak/com.vibehere.KeePassXC.OSPasskeys.yml"
APP_ID=com.vibehere.KeePassXC.OSPasskeys

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out-dir) OUT_DIR="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    --arch) ARCH="$2"; shift 2 ;;
    --manifest) MANIFEST="$2"; shift 2 ;;
    *) echo "unknown arg: $1"; exit 1 ;;
  esac
done

cd "$ROOT"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

case "$ARCH" in
  amd64|x86_64) ARCH=x86_64 ;;
  arm64|aarch64) ARCH=aarch64 ;;
esac

if ! command -v flatpak >/dev/null || ! command -v flatpak-builder >/dev/null; then
  echo "flatpak and flatpak-builder are required"
  exit 1
fi

flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install -y --user flathub "org.kde.Platform//6.9" "org.kde.Sdk//6.9"

BUILD_DIR="$OUT_DIR/flatpak-build"
REPO_DIR="$OUT_DIR/flatpak-repo"
rm -rf "$BUILD_DIR" "$REPO_DIR"
mkdir -p "$BUILD_DIR" "$REPO_DIR"

flatpak-builder --user --force-clean --repo="$REPO_DIR" \
  --state-dir="$OUT_DIR/flatpak-state" \
  "$BUILD_DIR" "$MANIFEST"

OUT_NAME="KeePassXC-OSPasskeys-${VERSION}-${ARCH}.flatpak"
OUT_PATH="$OUT_DIR/$OUT_NAME"
rm -f "$OUT_PATH"
flatpak build-bundle "$REPO_DIR" "$OUT_PATH" "$APP_ID" --arch="$ARCH"

# Ship udev rule next to the bundle for OS Passkeys on immutable hosts
cp -f "$ROOT/share/linux/udev/99-keepassxc-uhid.rules" "$OUT_DIR/99-keepassxc-uhid.rules"
sed -i 's/\r$//' "$OUT_DIR/99-keepassxc-uhid.rules"

ls -lh "$OUT_PATH" "$OUT_DIR/99-keepassxc-uhid.rules"
echo "FLATPAK_OK $OUT_PATH"
