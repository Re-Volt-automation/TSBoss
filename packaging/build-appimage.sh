#!/usr/bin/env bash
# Build a portable TSBoss AppImage.
# Downloads linuxdeploy + the Qt plugin into packaging/tools/ on first run,
# builds & installs TSBoss into an AppDir, then bundles everything.
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS="$PROJECT_ROOT/packaging/tools"
BUILD="$PROJECT_ROOT/build-appimage"
APPDIR="$BUILD/AppDir"
ARCH="$(uname -m)"

mkdir -p "$TOOLS"

fetch() { # url dest
    if [ ! -x "$2" ]; then
        echo ">> downloading $(basename "$2")"
        curl -L --fail -o "$2" "$1"
        chmod +x "$2"
    fi
}

LD="$TOOLS/linuxdeploy-${ARCH}.AppImage"
LDQT="$TOOLS/linuxdeploy-plugin-qt-${ARCH}.AppImage"
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage" "$LD"
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage" "$LDQT"

# Configure + build + install into AppDir
cmake -S "$PROJECT_ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD" --parallel
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD"

# Bundle Qt + dependencies and emit the AppImage
export QML_SOURCES_PATHS=""   # no QML in this app
export PATH="$TOOLS:$PATH"
cd "$BUILD"
OUTPUT="TSBoss-${ARCH}.AppImage" \
"$LD" --appdir "$APPDIR" \
    --plugin qt \
    --output appimage

echo
echo ">> done: $BUILD/TSBoss-${ARCH}.AppImage"
