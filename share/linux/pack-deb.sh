#!/bin/bash
# Build a .deb from an existing KeePassXC Linux build (or rebuild first).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC_WIN=${SRC_WIN:-$ROOT}
BUILD_ROOT=${BUILD_ROOT:-$HOME/keepassxc-linux-build}
SRC_COPY=${SRC_COPY:-$HOME/keepassxc-src}
OUT_DIR=$SRC_WIN/share/linux/out
STAGE=$HOME/keepassxc-deb-stage
PKG_NAME=keepassxc-ospasskeys
VERSION=2.8.0
ARCH=amd64
DEB_FILE=$OUT_DIR/${PKG_NAME}_${VERSION}_${ARCH}.deb

if [[ ! -x $BUILD_ROOT/src/keepassxc ]]; then
  echo "No build at $BUILD_ROOT — run share/linux/build-linux.sh first"
  exit 1
fi

echo "[1/4] install into staging..."
rm -rf "$STAGE"
mkdir -p "$STAGE"
DESTDIR="$STAGE" cmake --install "$BUILD_ROOT" --prefix /usr

# Ensure udev rule is present even if cmake install skipped it
mkdir -p "$STAGE/usr/lib/udev/rules.d"
cp -f "$SRC_WIN/share/linux/udev/99-keepassxc-uhid.rules" \
  "$STAGE/usr/lib/udev/rules.d/99-keepassxc-uhid.rules"
# strip CRLF if copied from Windows
sed -i 's/\r$//' "$STAGE/usr/lib/udev/rules.d/99-keepassxc-uhid.rules"

echo "[2/4] write Debian control..."
mkdir -p "$STAGE/DEBIAN"
SIZE_KB=$(du -sk "$STAGE/usr" | awk '{print $1}')
cat >"$STAGE/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: KeePassXC OS Passkeys Fork <noreply@local>
Installed-Size: ${SIZE_KB}
Depends: libqt6core6t64 | libqt6core6, libqt6gui6t64 | libqt6gui6, libqt6widgets6t64 | libqt6widgets6, libbotan-3-10 | libbotan-3-9 | libbotan-3-7, libc6
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
# ensure uhid module hint
if [ -e /etc/modules-load.d ] && [ ! -f /etc/modules-load.d/uhid-keepassxc.conf ]; then
  echo uhid > /etc/modules-load.d/uhid-keepassxc.conf 2>/dev/null || true
fi
exit 0
EOF
chmod 755 "$STAGE/DEBIAN/postinst"

echo "[3/4] build .deb..."
mkdir -p "$OUT_DIR"
# Also publish loose binaries for WSL/manual use
cp -f "$BUILD_ROOT/src/keepassxc" "$OUT_DIR/KeePassXC"
cp -f "$BUILD_ROOT/src/proxy/keepassxc-proxy" "$OUT_DIR/keepassxc-proxy" 2>/dev/null || true
cp -f "$BUILD_ROOT/src/cli/keepassxc-cli" "$OUT_DIR/keepassxc-cli" 2>/dev/null || true
chmod +x "$OUT_DIR/KeePassXC" "$OUT_DIR/keepassxc-proxy" "$OUT_DIR/keepassxc-cli" 2>/dev/null || true

dpkg-deb --root-owner-group --build "$STAGE" "$DEB_FILE"

echo "[4/4] verify package..."
dpkg-deb -I "$DEB_FILE"
dpkg-deb -c "$DEB_FILE" | head -40 || true
ls -lh "$DEB_FILE" "$OUT_DIR/KeePassXC"
echo "DEB_OK $DEB_FILE"
