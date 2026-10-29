#!/bin/bash
# Build a portable AppImage from an existing KeePassXC build (works on Bazzite, Fedora, Arch, etc.).
set -euo pipefail

BUILD_DIR=build
OUT_DIR=dist
VERSION=2.8.0
ARCH="$(uname -m)"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --out-dir) OUT_DIR="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    --arch) ARCH="$2"; shift 2 ;;
    *) echo "unknown arg: $1"; exit 1 ;;
  esac
done

cd "$ROOT"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

case "$ARCH" in
  amd64|x86_64) ARCH=x86_64; LD_ARCH=x86_64; MULTIARCH=x86_64-linux-gnu ;;
  arm64|aarch64) ARCH=aarch64; LD_ARCH=aarch64; MULTIARCH=aarch64-linux-gnu ;;
  *) echo "unsupported arch: $ARCH"; exit 1 ;;
esac

BIN="$BUILD_DIR/src/keepassxc"
if [[ ! -x "$BIN" ]]; then
  echo "missing binary: $BIN"
  exit 1
fi

TOOLS="$OUT_DIR/.appimage-tools-$LD_ARCH"
mkdir -p "$TOOLS"
export APPIMAGE_EXTRACT_AND_RUN=1

fetch_tool() {
  local url="$1"
  local dest="$2"
  if [[ ! -x "$dest" ]]; then
    curl -fsSL --retry 3 -o "$dest" "$url"
    chmod +x "$dest"
  fi
}

fetch_tool \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LD_ARCH}.AppImage" \
  "$TOOLS/linuxdeploy"
fetch_tool \
  "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${LD_ARCH}.AppImage" \
  "$TOOLS/linuxdeploy-plugin-qt"
fetch_tool \
  "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${LD_ARCH}.AppImage" \
  "$TOOLS/appimagetool"

APPDIR="$OUT_DIR/KeePassXC-OSPasskeys.AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr

mkdir -p "$APPDIR/usr/lib/udev/rules.d"
cp -f "$ROOT/share/linux/udev/99-keepassxc-uhid.rules" \
  "$APPDIR/usr/lib/udev/rules.d/99-keepassxc-uhid.rules"
sed -i 's/\r$//' "$APPDIR/usr/lib/udev/rules.d/99-keepassxc-uhid.rules"

DESKTOP_SRC="$APPDIR/usr/share/applications/org.keepassxc.KeePassXC.desktop"
if [[ ! -f "$DESKTOP_SRC" ]]; then
  echo "missing desktop file after install"
  exit 1
fi
cp -f "$DESKTOP_SRC" "$APPDIR/org.keepassxc.KeePassXC.desktop"
sed -i 's/^Name=.*/Name=KeePassXC OS Passkeys/' "$APPDIR/org.keepassxc.KeePassXC.desktop"
sed -i 's/^Name=.*/Name=KeePassXC OS Passkeys/' "$DESKTOP_SRC" || true

ICON_SRC="$APPDIR/usr/share/icons/hicolor/256x256/apps/keepassxc.png"
if [[ -f "$ICON_SRC" ]]; then
  cp -f "$ICON_SRC" "$APPDIR/keepassxc.png"
fi

cp -f "$ROOT/share/linux/appimage-apprun.sh" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"

# Debian Qt6 sometimes omits libqsvgicon.so where linuxdeploy-plugin-qt expects it.
EXPECT_SVG="/usr/lib/${MULTIARCH}/qt6/plugins/iconengines/libqsvgicon.so"
if [[ ! -e "$EXPECT_SVG" ]]; then
  FOUND_SVG="$(find /usr/lib /usr/lib64 -name 'libqsvgicon.so' 2>/dev/null | head -1 || true)"
  mkdir -p "$(dirname "$EXPECT_SVG")"
  if [[ -n "$FOUND_SVG" ]]; then
    ln -sfn "$FOUND_SVG" "$EXPECT_SVG"
  else
    echo "libqsvgicon.so missing — deploying Qt plugins manually (no linuxdeploy qt plugin)"
    USE_MANUAL_QT=1
  fi
fi

export QMAKE="$(command -v qmake6 || command -v qmake || true)"
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"
export VERSION

LD_ARGS=(
  --appimage-extract-and-run
  --appdir "$APPDIR"
  --desktop-file "$APPDIR/org.keepassxc.KeePassXC.desktop"
  --executable "$APPDIR/usr/bin/keepassxc"
  --executable "$APPDIR/usr/bin/keepassxc-cli"
  --executable "$APPDIR/usr/bin/keepassxc-proxy"
)
if [[ -f "$ICON_SRC" ]]; then
  LD_ARGS+=(--icon-file "$ICON_SRC")
fi

if [[ "${USE_MANUAL_QT:-0}" == "1" ]]; then
  "$TOOLS/linuxdeploy" "${LD_ARGS[@]}"
  QT_PLUGINS="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
  if [[ -z "$QT_PLUGINS" || ! -d "$QT_PLUGINS" ]]; then
    QT_PLUGINS="/usr/lib/${MULTIARCH}/qt6/plugins"
  fi
  mkdir -p "$APPDIR/usr/plugins"
  for plug in platforms imageformats tls networkinformation sqldrivers iconengines styles generic; do
    if [[ -d "$QT_PLUGINS/$plug" ]]; then
      cp -a "$QT_PLUGINS/$plug" "$APPDIR/usr/plugins/"
    fi
  done
  # Also pull in Qt libs referenced by plugins
  while IFS= read -r so; do
    [[ -f "$so" ]] || continue
    "$TOOLS/linuxdeploy" --appimage-extract-and-run --appdir "$APPDIR" --library "$so" || true
  done < <(find "$APPDIR/usr/plugins" -type f -name '*.so' | head -80)
else
  "$TOOLS/linuxdeploy" "${LD_ARGS[@]}" --plugin qt
fi

# Point Qt at bundled plugins inside the AppImage
cat >"$APPDIR/usr/bin/qt.conf" <<'EOF'
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
EOF

OUT_NAME="KeePassXC-OSPasskeys-${VERSION}-${ARCH}.AppImage"
OUT_PATH="$OUT_DIR/$OUT_NAME"
rm -f "$OUT_PATH"

ARCH="$LD_ARCH" "$TOOLS/appimagetool" --appimage-extract-and-run \
  -n "$APPDIR" "$OUT_PATH"

chmod +x "$OUT_PATH"
ls -lh "$OUT_PATH"
echo "APPIMAGE_OK $OUT_PATH"
