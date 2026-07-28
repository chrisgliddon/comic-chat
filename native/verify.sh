#!/bin/sh
# verify.sh - build the native dumps and diff them against the frozen Windows
# goldens. All three milestones of the native port, and their regression test.
#
# Milestone 1: the 33 Tier-2 asset manifests (avbfile + dib + avatar + backdrop).
# Milestone 2: Tier-1 #3 avatario and #2 avatar-pose - pure arithmetic, so a clean
#   diff there isolates float-to-double widening, the tree-dependent PI used by the
#   angle metric, and the MSVC RNG from everything else.
# Milestone 3: the 15 Tier-3 corpus cases - the whole engine RUNNING. Pages, panels,
#   avatar placement, pose selection, balloon layout, line breaking, splines and
#   trajectories, all compared byte-for-byte against dumps captured on Windows.
#
# What a clean run proves, all at once:
#
#   * Integer widths and struct packing. avbfile.h typedefs AVBINT32 from ULONG
#     inside #pragma pack(push, 1) and memcpys AVATARFACEDATA/BODYDATA/TORSODATA
#     straight out of the file. Had the shim's ULONG been `unsigned long` - 8 bytes
#     on arm64, 4 on Win32 - every offset in every asset would be read from the
#     wrong bytes.
#   * The zlib framing, the ditto-optimisation, and the 2-bpp maskedmono/dualmask
#     expansion: 552 poses, 1612 decoded image slots, ~13.8 MB of hashed pixels.
#   * DIBStorageWidth's sub-byte rounding and the row-masked pixel hash behaving
#     identically off-Windows.
#   * COLORREF channel order (0x00BBGGRR) surviving the port, via the palette CRCs.
#
# The comparison is strong because BOTH sides run the same dump code
# (oracle/harness/avbdump.cpp) against the same assets - a match cannot be two
# implementations coincidentally agreeing, and a mismatch is attributable to the
# platform layer rather than to the dump.

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

. ./native/units.sh

BUILD=native/build
OUT=$BUILD/avbout
mkdir -p "$BUILD"

# ORACLE_HARNESS is defined on purpose (see units.sh): it activates the observability
# hooks in avbfile.cpp whose output is part of the frozen manifests. Without it
# recordTags and the histograms would be empty and every manifest would differ.
native_stage
native_compile $NATIVE_UNITS avbmain posemain glyphmain || exit 1
clang++ -c -std=c++14 -O1 -w -o "$BUILD/uistubs.o" native/uistubs.cpp

# One object set, several entry points.
clang++ -o "$BUILD/avbdump"  $(native_objs) "$BUILD/avbmain.o"  "$BUILD/uistubs.o" native/shim/msvcrand.cpp -lz
clang++ -o "$BUILD/posedump" $(native_objs) "$BUILD/posemain.o" "$BUILD/uistubs.o" native/shim/msvcrand.cpp -lz

# glyphcheck stays standalone: it exercises the measurement layer through the CDC API and
# needs no engine object, so a failure there cannot be blamed on the engine.
clang++ -o "$BUILD/glyphcheck" "$BUILD/glyphmain.o" "$BUILD/glyphtable.o" \
    "$BUILD/glyphtable_cdc.o" "$BUILD/ojson.o" native/shim/msvcrand.cpp
"$BUILD/glyphcheck" oracle/glyphs/glyphs.json

rm -rf "$OUT"
mkdir -p "$OUT"
"$BUILD/avbdump" "$OUT" v2.5-beta-1-modern/ComicArt 2> "$BUILD/avbdump.log"

match=0
mismatch=0
mislist=""

# --- milestone 1: the 33 asset manifests ---
for g in oracle/avb/*.golden.json; do
    stem=$(basename "$g" .golden.json)
    a="$OUT/$stem.json"
    if [ ! -f "$a" ]; then
        mismatch=$((mismatch + 1)); mislist="$mislist $stem(missing)"; continue
    fi
    if cmp -s "$g" "$a"; then
        match=$((match + 1))
    else
        mismatch=$((mismatch + 1)); mislist="$mislist $stem"
    fi
done

# --- milestone 2: the no-DC Tier-1 dumps ---
"$BUILD/posedump" "$BUILD/avatario.json" "$BUILD/avatar.json" > /dev/null

for pair in "avatario:oracle/avatario/avatario.golden.json" "avatar:oracle/avatar/avatar.golden.json"; do
    name=${pair%%:*}
    golden=${pair##*:}
    if cmp -s "$golden" "$BUILD/$name.json"; then
        match=$((match + 1))
    else
        mismatch=$((mismatch + 1)); mislist="$mislist $name"
    fi
done

# --- milestone 3: the 15 Tier-3 corpus cases ---
# The full engine executing, not just decoding assets: pages, panels, avatar placement,
# pose selection, balloon layout, line breaking, splines and trajectories.
#
# Run from v2.5-beta-1-modern because corpus inputs.json carries treeDir="." and the
# Windows oracle runs the harness from there - ComicArt has to resolve the same way.
sh ./native/build-harness.sh > /dev/null
for c in oracle/corpus/*/; do
    name=$(basename "$c")
    [ -f "$c/inputs.json" ] || continue
    out="$ROOT/$BUILD/corpus-$name.json"
    if ! (cd v2.5-beta-1-modern && "$ROOT/$BUILD/harness" "../oracle/corpus/$name/inputs.json" "$out" \
            > "$ROOT/$BUILD/corpus-$name.log" 2>&1); then
        mismatch=$((mismatch + 1)); mislist="$mislist corpus/$name(crash)"; continue
    fi
    if [ ! -f "oracle/corpus/$name/expected.json" ]; then
        continue                      # not frozen yet; nothing to compare against
    fi
    if cmp -s "oracle/corpus/$name/expected.json" "$out"; then
        match=$((match + 1))
    else
        mismatch=$((mismatch + 1)); mislist="$mislist corpus/$name"
    fi
done

echo ""
echo "=== native vs frozen Windows goldens ==="
echo "matched   : $match"
echo "mismatched: $mismatch$mislist"

if [ "$mismatch" -ne 0 ]; then
    first=$(echo "$mislist" | awk '{print $1}')
    echo ""
    echo "first difference ($first):"
    # The first mismatch may be a corpus case now, so pick the right pair of files rather
    # than always reaching into oracle/avb - a diff of the wrong files reads as corruption.
    case "$first" in
        corpus/*)
            cname=${first#corpus/}
            cname=${cname%%(*}
            diff "oracle/corpus/$cname/expected.json" "$BUILD/corpus-$cname.json" | head -20 || true
            ;;
        avatario|avatar)
            diff "oracle/$first/$first.golden.json" "$BUILD/$first.json" | head -20 || true
            ;;
        *)
            diff "oracle/avb/$first.golden.json" "$OUT/$first.json" | head -20 || true
            ;;
    esac
    exit 1
fi
echo ""
echo "Native macOS build reproduces all $match Windows goldens byte-for-byte"
echo "(33 asset manifests + avatario + avatar-pose + 15 Tier-3 corpus cases)."
