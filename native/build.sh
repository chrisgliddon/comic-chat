#!/bin/sh
# build.sh - compile the engine natively on macOS and report per-file status.
#
# Not a build of the app yet. This is the instrument that tells you how much of
# the engine the shim layer currently supports; run it after touching anything in
# native/shim to see what moved. It compiles to objects only - there is no link
# step until a milestone target exists.
#
# Usage:  native/build.sh            # status table for the engine-core set
#         native/build.sh <file>...  # detail (full error text) for named files
#
# The flag set matters:
#   -fms-extensions   MSVC allows redundant member qualification (void X::f() {}
#                     inside class X) which the engine uses. clang needs telling.
#                     NOTE: -fms-compatibility is deliberately NOT used - it
#                     redefines va_list against macOS system headers and breaks
#                     every translation unit.
#   -I native/shim    the shim floor, plus windows.h/tchar.h stand-ins that the
#                     vendored artifacts/inc headers reach for with <angles>.
#   -w                the engine predates most of these warnings; the Windows
#                     build is equally noisy and the signal here is errors only.

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p native/build

CXXFLAGS="-std=c++14 -O1 -w -fms-extensions -I native/shim -I artifacts/inc"

# The engine-core set: zero MFC-UI coupling, and the subsystems the oracle
# already has frozen goldens for. Deliberately does NOT include the dialog/OLE
# files - those are not part of a native app at all.
CORE="vector2d bbox traj arc spline splinutl dib avbfile backdrop avatar avatario textpose format userinfo doskey"

./native/stage.sh > /dev/null

if [ $# -gt 0 ]; then
    for f in "$@"; do
        echo "=== $f ==="
        clang++ -c $CXXFLAGS -o "native/build/$f.o" "native/stage/$f.cpp" 2>&1 | head -40
    done
    exit 0
fi

ok=0; fail=0
for f in $CORE; do
    if out=$(clang++ -c $CXXFLAGS -o "native/build/$f.o" "native/stage/$f.cpp" 2>&1); then
        printf "  %-11s OK\n" "$f"
        ok=$((ok + 1))
    else
        n=$(printf '%s' "$out" | grep -c 'error:' || true)
        first=$(printf '%s' "$out" | grep 'error:' | head -1 | sed 's|native/stage/||; s|.*error: ||' | cut -c1-52)
        printf "  %-11s %-3s errors  %s\n" "$f" "$n" "$first"
        fail=$((fail + 1))
    fi
done
echo ""
echo "engine core: $ok compiling, $fail blocked"
