#!/bin/bash
set -euo pipefail

BUILD_DIR=build
OUT_DIR=dist
VERSION=2.8.0
PKG_NAME=keepassxc-ospasskeys
ARCH="$(dpkg --print-architecture 2>/dev/null || uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --out-dir) OUT_DIR="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    --pkg-name) PKG_NAME="$2"; shift 2 ;;
    --arch) ARCH="$2"; shift 2 ;;
    *) echo "unknown arg: $1"; exit 1 ;;
  esac
done

cd "$ROOT"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

BIN="$BUILD_DIR/src/keepassxc"
if [[ ! -x "$BIN" ]]; then
  echo "missing binary: $BIN"
  exit 1
fi

DESTDIR="$STAGE" cmake --install "$BUILD_DIR" --prefix /usr

mkdir -p "$STAGE/usr/lib/udev/rules.d"
cp -f "$ROOT/share/linux/udev/99-keepassxc-uhid.rules" \
  "$STAGE/usr/lib/udev/rules.d/99-keepassxc-uhid.rules"
sed -i 's/\r$//' "$STAGE/usr/lib/udev/rules.d/99-keepassxc-uhid.rules"

mkdir -p "$STAGE/DEBIAN"
SIZE_KB=$(du -sk "$STAGE/usr" | awk '{print $1}')

# Prefer auto-generated shared-library Depends when dpkg-shlibdeps is available.
DEPENDS="libqt6core6t64 | libqt6core6, libqt6gui6t64 | libqt6gui6, libqt6widgets6t64 | libqt6widgets6, libqt6network6t64 | libqt6network6, libqt6svg6, libbotan-3-10 | libbotan-3-9 | libbotan-3-7 | libbotan-3-2 | libbotan-2-19, libc6, libminizip1t64 | libminizip1, libpcsclite1, libusb-1.0-0, libqrencode4, libxkbcommon0, libxi6, libxtst6"
if command -v dpkg-shlibdeps >/dev/null 2>&1; then
  mkdir -p "$STAGE/debian"
  printf 'Source: %s\nPackage: %s\nArchitecture: %s\nDepends: ${shlibs:Depends}\n' \
    "$PKG_NAME" "$PKG_NAME" "$ARCH" >"$STAGE/debian/control"
  if AUTO_DEPS=$(cd "$STAGE" && dpkg-shlibdeps -O -e usr/bin/keepassxc -e usr/bin/keepassxc-cli -e usr/bin/keepassxc-proxy 2>/dev/null | sed 's/^shlibs:Depends=//'); then
    if [[ -n "$AUTO_DEPS" ]]; then
      DEPENDS="$AUTO_DEPS"
    fi
  fi
  rm -rf "$STAGE/debian"
fi

cat >"$STAGE/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: KeePassXC OS Passkeys Fork <noreply@local>
Installed-Size: ${SIZE_KB}
Depends: ${DEPENDS}
Recommends: keepassxc-browser
Description: KeePassXC with OS passkeys (Linux UHID FIDO2) and browser integration
 Privacy-first KeePassXC fork with optional virtual FIDO2 OS passkey provider
 (/dev/uhid) and full KeePassXC-Browser compatibility.
EOF

cat >"$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ -d /run/udev ] || [ -d /etc/udev ]; then
  udevadm control --reload-rules 2>/dev/null || true
  udevadm trigger --name-match=uhid 2>/dev/null || true
fi
if [ -e /etc/modules-load.d ] && [ ! -f /etc/modules-load.d/uhid-keepassxc.conf ]; then
  echo uhid > /etc/modules-load.d/uhid-keepassxc.conf 2>/dev/null || true
fi
exit 0
EOF
chmod 755 "$STAGE/DEBIAN/postinst"

DEB_FILE="$OUT_DIR/${PKG_NAME}_${VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$STAGE" "$DEB_FILE"

cp -f "$BUILD_DIR/src/keepassxc" "$OUT_DIR/KeePassXC"
cp -f "$BUILD_DIR/src/cli/keepassxc-cli" "$OUT_DIR/keepassxc-cli" 2>/dev/null || true
cp -f "$BUILD_DIR/src/proxy/keepassxc-proxy" "$OUT_DIR/keepassxc-proxy" 2>/dev/null || true
chmod +x "$OUT_DIR/KeePassXC" "$OUT_DIR/keepassxc-cli" "$OUT_DIR/keepassxc-proxy" 2>/dev/null || true

# Standalone udev rule + full linux bundle for the GitHub Release
cp -f "$ROOT/share/linux/udev/99-keepassxc-uhid.rules" "$OUT_DIR/99-keepassxc-uhid.rules"
sed -i 's/\r$//' "$OUT_DIR/99-keepassxc-uhid.rules"

TAR_NAME="${PKG_NAME}_${VERSION}_${ARCH}.tar.gz"
tar -C "$OUT_DIR" -czf "$OUT_DIR/$TAR_NAME" \
  KeePassXC keepassxc-cli keepassxc-proxy \
  "99-keepassxc-uhid.rules" \
  "$(basename "$DEB_FILE")"

(
  cd "$OUT_DIR"
  sha256sum \
    "$(basename "$DEB_FILE")" \
    "$TAR_NAME" \
    KeePassXC keepassxc-cli keepassxc-proxy \
    99-keepassxc-uhid.rules \
    > SHA256SUMS
)

ls -lh "$DEB_FILE" "$OUT_DIR/KeePassXC" "$OUT_DIR/$TAR_NAME" "$OUT_DIR/SHA256SUMS"
echo "DEB_OK $DEB_FILE"
