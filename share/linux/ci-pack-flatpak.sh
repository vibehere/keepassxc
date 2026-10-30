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

if ! command -v flatpak >/dev/null; then
  echo "flatpak is required"
  exit 1
fi

# Some environments inject CCACHE_DIR under a transient /tmp path. bubblewrap
# inside flatpak-builder fails if that path is missing — drop it for the build.
if [[ -n "${CCACHE_DIR:-}" && ! -d "${CCACHE_DIR}" ]]; then
  unset CCACHE_DIR
fi
unset CCACHE_BASEDIR || true

# Prefer host flatpak-builder; on immutable hosts use Flathub org.flatpak.Builder.
FB=()
FB_EXTRA=()
if command -v flatpak-builder >/dev/null; then
  FB=(flatpak-builder)
  # Host builder: keep artifacts in the user installation.
  FB_EXTRA=(--user)
elif flatpak info --user org.flatpak.Builder &>/dev/null \
  || flatpak info org.flatpak.Builder &>/dev/null; then
  # Builder-as-Flatpak sees system runtimes more reliably than --user ones.
  FB=(flatpak run --command=flatpak-builder --filesystem=home --share=network org.flatpak.Builder)
else
  echo "flatpak-builder not found; install it or: flatpak install flathub org.flatpak.Builder"
  exit 1
fi

flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
# Prefer system SDK when possible (needed for org.flatpak.Builder); fall back to user.
if ! flatpak info --system "org.kde.Sdk//6.9" &>/dev/null; then
  flatpak install -y --system flathub "org.kde.Platform//6.9" "org.kde.Sdk//6.9" \
    || flatpak install -y --user flathub "org.kde.Platform//6.9" "org.kde.Sdk//6.9"
fi

BUILD_DIR="$OUT_DIR/flatpak-build"
REPO_DIR="$OUT_DIR/flatpak-repo"
rm -rf "$BUILD_DIR" "$REPO_DIR"
mkdir -p "$BUILD_DIR" "$REPO_DIR"

"${FB[@]}" "${FB_EXTRA[@]}" --force-clean --install-deps-from=flathub \
  --repo="$REPO_DIR" \
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
