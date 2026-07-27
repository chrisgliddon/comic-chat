#!/bin/sh
# stage.sh - build a staging tree where the engine sources sit beside the SHIM
# stdafx.h instead of the MFC one.
#
# Why staging rather than include paths: a quoted #include is resolved relative to
# the including file's own directory first, before any -I. Every engine .cpp opens
# with #include "stdafx.h", so compiling them in place always picks up
# v2.5-beta-1-modern/stdafx.h - which has no include guard to define around, and
# which pulls in afxwin/afxext/afxole/afxsock plus chicdial.h and coolbar.h. That
# last part is the real blocker: v2.5's stdafx.h drags the MFC *UI* surface
# (CDialog, CToolBar) into every translation unit, including ones that have no
# business needing it.
#
# Symlinking the sources into a directory that contains our own stdafx.h makes the
# quoted lookup find the shim first, with zero edits to the engine tree. Headers
# are symlinks so edits to the originals are picked up immediately; only stdafx.h
# is ours.
#
# Re-runnable: it clears and rebuilds the link farm each time.

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ENGINE="$ROOT/v2.5-beta-1-modern"
SHIM="$ROOT/native/shim"
STAGE="$ROOT/native/stage"

rm -rf "$STAGE"
mkdir -p "$STAGE"

# Engine headers and sources, minus stdafx.h (ours wins) and stdafx.cpp (it only
# exists to build the MFC precompiled header).
for f in "$ENGINE"/*.h "$ENGINE"/*.cpp; do
    base=$(basename "$f")
    [ "$base" = "stdafx.h" ] && continue
    [ "$base" = "stdafx.cpp" ] && continue
    ln -sf "$f" "$STAGE/$base"
done

# The shim, including the stdafx.h that replaces the engine's.
for f in "$SHIM"/*.h; do
    ln -sf "$f" "$STAGE/$(basename "$f")"
done

# Oracle harness sources so the native build can produce the same dumps and be
# diffed against the frozen Windows goldens - the whole verification strategy.
for f in "$ROOT/oracle/harness"/*.h "$ROOT/oracle/harness"/*.cpp; do
    ln -sf "$f" "$STAGE/$(basename "$f")"
done

echo "staged $(ls "$STAGE" | wc -l | tr -d ' ') files in native/stage"
