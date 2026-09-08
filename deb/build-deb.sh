#!/bin/bash
# Build the dock-lyrics .deb package.
# Usage: bash deb/build-deb.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="$ROOT/deb/dock-lyrics"
BUILD="$ROOT/build"

# sanity: require a fresh build
[ -f "$BUILD/plugins/org.deepin.ds.dock.lyrics.so" ] || { echo "build first: cmake --build build"; exit 1; }

rm -rf "$PKG/usr"
mkdir -p "$PKG/usr/lib/x86_64-linux-gnu/dde-shell" "$PKG/usr/share/dde-shell/org.deepin.ds.dock.lyrics"

cp -f  "$BUILD/plugins/org.deepin.ds.dock.lyrics.so" "$PKG/usr/lib/x86_64-linux-gnu/dde-shell/"
cp -rf "$BUILD/packages/org.deepin.ds.dock.lyrics/." "$PKG/usr/share/dde-shell/org.deepin.ds.dock.lyrics/"

dpkg-deb --build --root-owner-group "$PKG" "$ROOT/dock-lyrics_1.0.0_amd64.deb"
rm -rf "$PKG/usr"   # drop payload copies; keep only the skeleton
echo "package ready: $ROOT/dock-lyrics_1.0.0_amd64.deb"
