#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC=${SRC:-$ROOT}
BUILD_ROOT=${BUILD_ROOT:-$HOME/keepassxc-linux-build}
SRC_COPY=${SRC_COPY:-$HOME/keepassxc-src}
OUT=$SRC/share/linux/out

echo "[1/5] ensure keyutils link/header..."
if [[ ! -e /usr/lib/x86_64-linux-gnu/libkeyutils.so ]]; then
  sudo ln -sf libkeyutils.so.1 /usr/lib/x86_64-linux-gnu/libkeyutils.so
fi
if [[ ! -f /usr/include/keyutils.h ]]; then
  sudo curl -fsSL --max-time 30 -o /usr/include/keyutils.h \
    'https://git.kernel.org/pub/scm/linux/kernel/git/dhowells/keyutils.git/plain/keyutils.h?h=v1.6.3'
fi

echo "[2/5] sync sources to Linux FS..."
rm -rf "$SRC_COPY"
mkdir -p "$SRC_COPY"
rsync -a --delete \
  --exclude 'build/' \
  --exclude 'share/msix/staging/' \
  --exclude 'share/msix/out/' \
  --exclude '.git/' \
  "$SRC/" "$SRC_COPY/"

echo "[3/5] cmake configure..."
rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT"
cd "$BUILD_ROOT"
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF \
  -DWITH_GUI_TESTS=OFF \
  -DKPXC_FEATURE_BROWSER=ON \
  -DKPXC_FEATURE_PASSKEYS=ON \
  -DKPXC_FEATURE_OS_PASSKEYS=ON \
  -DKPXC_FEATURE_SSHAGENT=ON \
  -DKPXC_FEATURE_FDOSECRETS=ON \
  "$SRC_COPY"

echo "[4/5] build..."
cmake --build . -j"$(nproc)"

echo "[5/5] package output..."
mkdir -p "$OUT"
BIN=$(find . -type f -name keepassxc -executable | head -n1)
echo "binary=$BIN"
cp -f "$BIN" "$OUT/KeePassXC"
chmod +x "$OUT/KeePassXC"
ls -lh "$OUT/KeePassXC"
"$OUT/KeePassXC" --version || true
echo "LINUX_BUILD_OK $OUT/KeePassXC"
