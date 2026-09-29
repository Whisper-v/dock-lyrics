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
APPID="com.github.dock-lyrics"
VERSION="1.1.1"
OUT="$ROOT/${APPID}_${VERSION}_amd64.deb"

# sanity: require a fresh build
[ -f "$BUILD/plugins/${APPID}.so" ] || { echo "build first: cmake --build build"; exit 1; }

# --- reproducible packaging timestamp -------------------------------------
if [ -z "${SOURCE_DATE_EPOCH:-}" ]; then
    SOURCE_DATE_EPOCH="$(git -C "$ROOT" log -1 --format=%ct 2>/dev/null || true)"
fi
[ -n "${SOURCE_DATE_EPOCH:-}" ] || SOURCE_DATE_EPOCH=0
export SOURCE_DATE_EPOCH
echo "SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH ($(date -d "@$SOURCE_DATE_EPOCH" '+%F %T %Z' 2>/dev/null || echo 'unix epoch'))"

rm -rf "$PKG/usr"
mkdir -p "$PKG/usr/lib/x86_64-linux-gnu/dde-shell" \
         "$PKG/usr/share/dde-shell/${APPID}" \
         "$PKG/usr/share/applications" \
         "$PKG/usr/share/icons/hicolor"

# 1) the plugin shared object
cp -f  "$BUILD/plugins/${APPID}.so" "$PKG/usr/lib/x86_64-linux-gnu/dde-shell/"
# 2) QML package (main.qml + metadata.json)
cp -rf "$BUILD/packages/${APPID}/." "$PKG/usr/share/dde-shell/${APPID}/"
# 3) desktop entry — REQUIRED by store/package validators so that the
#    Icon= key can be resolved to a real file inside the package.
install -m 644 "$ROOT/data/${APPID}.desktop" "$PKG/usr/share/applications/"
# 4) icon set in the freedesktop hicolor theme, one file per size.
#    The file name must equal the desktop Icon= value (no extension).
for s in 16 24 32 48 64 128 256; do
    d="$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
    mkdir -p "$d"
    install -m 644 "$ROOT/data/icons/hicolor/${s}x${s}/apps/${APPID}.png" \
                   "$d/${APPID}.png"
done
# pin mtimes so the tarball metadata is stable as well
find "$PKG" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +

dpkg-deb --build --root-owner-group "$PKG" "$OUT"
rm -rf "$PKG/usr"   # drop payload copies; keep only the skeleton
echo "package ready: $OUT"
echo "sha256: $(sha256sum "$OUT" | cut -d' ' -f1)"
