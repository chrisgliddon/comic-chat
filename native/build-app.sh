#!/bin/sh
# build-app.sh - build "Comic Chat.app".
#
#     ./native/build-app.sh              -> native/build/Comic Chat.app
#     ./native/build-app.sh --install    -> also copies it to /Applications
#
# The bundle is self-contained: ComicArt and glyphs.json go into
# Resources, so the installed app has no dependency on this source tree.
#
# Run under sh, not zsh - see the note in units.sh about word splitting.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
. ./native/units.sh

BUILD=native/build
APP="$BUILD/Comic Chat.app"

mkdir -p "$BUILD"
native_stage
native_compile $NATIVE_UNITS || exit 1
clang++ -c -std=c++14 -O1 -w -o "$BUILD/uistubs.o" native/uistubs.cpp

# The front end is Objective-C++ and lives outside the staging tree: it includes only
# native/session.h and native/render.h, never an engine header, which is the seam that keeps
# Cocoa and MFC-shim code from having to agree about anything.
clang++ -c -std=c++14 -fobjc-arc -O1 -w -I native/shim -o "$BUILD/appmain.o" native/app/main.mm

clang++ -o "$BUILD/comicchat" $(native_objs) "$BUILD/appmain.o" "$BUILD/uistubs.o" \
    native/shim/msvcrand.cpp -lz $NATIVE_FRAMEWORKS \
    -framework Cocoa -framework AppKit

# --- bundle ---------------------------------------------------------------------------
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BUILD/comicchat" "$APP/Contents/MacOS/comicchat"

cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>              <string>Comic Chat</string>
    <key>CFBundleDisplayName</key>       <string>Comic Chat</string>
    <key>CFBundleIdentifier</key>        <string>org.comicchat.native</string>
    <key>CFBundleExecutable</key>        <string>comicchat</string>
    <key>CFBundlePackageType</key>       <string>APPL</string>
    <key>CFBundleShortVersionString</key><string>2.5</string>
    <key>CFBundleVersion</key>           <string>2.5</string>
    <key>LSMinimumSystemVersion</key>    <string>11.0</string>
    <key>NSHighResolutionCapable</key>   <true/>
    <key>NSPrincipalClass</key>          <string>NSApplication</string>
</dict>
</plist>
PLIST

# The engine's art and the frozen data files. ComicArt is ~9 MB of .avb/.bgb assets; copying
# rather than symlinking is the point - an installed app must not reach back into a checkout
# that may move or disappear.
cp -R v2.5-beta-1-modern/ComicArt "$APP/Contents/Resources/ComicArt"
cp oracle/glyphs/glyphs.json      "$APP/Contents/Resources/glyphs.json"

# chat.rc's resources need no copying: the string table and every BITMAP/DIB/ICON are compiled
# into the binary by native/gen-rcdata.py, which is what rc.exe and the linker did for the
# original. glyphs.json is the one genuine data file, because it is not a resource - it is the
# frozen measurement oracle RULEBOOK 5 requires, and it never existed in the 1996 build.

echo "built $APP"
du -sh "$APP" | sed 's/^/  size: /'

if [ "$1" = "--install" ]; then
    DEST="/Applications/Comic Chat.app"
    rm -rf "$DEST"
    cp -R "$APP" "$DEST"
    echo "installed $DEST"
fi
