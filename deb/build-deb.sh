#!/bin/bash
# Build the dock-lyrics .deb package.
# Usage: bash deb/build-deb.sh
#
# Reproducible packaging: every file timestamp inside the .deb is pinned to
# SOURCE_DATE_EPOCH, so rebuilding the same commit yields a byte-identical
# package. Default is the HEAD commit time; override it by exporting
# SOURCE_DATE_EPOCH, e.g.  SOURCE_DATE_EPOCH=0 bash deb/build-deb.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="$ROOT/deb/com.github.dock-lyrics"
BUILD="$ROOT/build"
OUT="$ROOT/com.github.dock-lyrics_1.0.0_amd64.deb"

# sanity: require a fresh build
[ -f "$BUILD/plugins/com.github.dock-lyrics.so" ] || { echo "build first: cmake --build build"; exit 1; }

# --- reproducible packaging timestamp -------------------------------------
# dpkg-deb honours SOURCE_DATE_EPOCH. Default to the HEAD commit time so that
# the same source always produces the same bytes.
if [ -z "${SOURCE_DATE_EPOCH:-}" ]; then
    SOURCE_DATE_EPOCH="$(git -C "$ROOT" log -1 --format=%ct 2>/dev/null || true)"
fi
[ -n "${SOURCE_DATE_EPOCH:-}" ] || SOURCE_DATE_EPOCH=0
export SOURCE_DATE_EPOCH
echo "SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH ($(date -d "@$SOURCE_DATE_EPOCH" '+%F %T %Z' 2>/dev/null || echo 'unix epoch'))"

rm -rf "$PKG/usr"
mkdir -p "$PKG/usr/lib/x86_64-linux-gnu/dde-shell" "$PKG/usr/share/dde-shell/com.github.dock-lyrics"

cp -f  "$BUILD/plugins/com.github.dock-lyrics.so" "$PKG/usr/lib/x86_64-linux-gnu/dde-shell/"
cp -rf "$BUILD/packages/com.github.dock-lyrics/." "$PKG/usr/share/dde-shell/com.github.dock-lyrics/"

# pin mtimes so the tarball metadata is stable as well
find "$PKG" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +

dpkg-deb --build --root-owner-group "$PKG" "$OUT"
rm -rf "$PKG/usr"   # drop payload copies; keep only the skeleton
echo "package ready: $OUT"
echo "sha256: $(sha256sum "$OUT" | cut -d' ' -f1)"
