#!/bin/sh
# verify.sh - build the native dumps and diff them against the frozen Windows
# goldens. Milestones 1 and 2 of the native port, and their regression test.
#
# Milestone 1: the 33 Tier-2 asset manifests (avbfile + dib + avatar + backdrop).
# Milestone 2: Tier-1 #3 avatario and #2 avatar-pose - pure arithmetic, so a clean
#   diff there isolates float-to-double widening, the tree-dependent PI used by the
#   angle metric, and the MSVC RNG from everything else.
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

CXXFLAGS="-std=c++14 -O1 -w -Wno-error=non-pod-varargs -fms-extensions -DORACLE_HARNESS -I native/shim -I artifacts/inc"
BUILD=native/build
OUT=$BUILD/avbout

# ORACLE_HARNESS is defined on purpose: it activates the observability hooks in
# avbfile.cpp (tag stream, image reader, pre-convert compression) whose output is
# part of the frozen manifests. Without it the native dump would legitimately
# differ - recordTags and the histograms would be empty.

./native/stage.sh > /dev/null
for c in avbmain posemain nativeglue nativeapp glyphmain; do
    ln -sf "$ROOT/native/$c.cpp" "native/stage/$c.cpp"
done

mkdir -p "$BUILD"

# Engine objects the dump needs, plus the harness dump itself. Deliberately NOT
# userinfo.o: it compiles, but importing it drags in the history and protocol layer
# (AddAndExecute, GetMembers, the HistoryEntry vtables) for a dump that needs none
# of it - native/nativeglue.cpp stubs the one symbol it was wanted for.
# doskey is LINKED rather than stubbed: it compiles, and CChatApp holds a CDosKey
# by value so its ctor runs.
# format is linked for CopyFormatting/FreeAndNullFormatting, which CChatApp's
# member teardown reaches.
# urlutil is linked for CUrlRec, which format.cpp's URL detection uses.
ENGINE="avbfile dib avatar backdrop avatario vector2d bbox doskey format urlutil"
SHARED="avbdump posedump ojson oracleseed nativeglue nativeapp"
# The measurement layer: the frozen glyph table plus CDC's measurement methods.
GLYPH="glyphtable glyphtable_cdc"
DRIVERS="avbmain posemain"

for c in glyphtable glyphtable_cdc; do
    ln -sf "$ROOT/native/shim/$c.cpp" "native/stage/$c.cpp"
done

for u in $ENGINE $SHARED $GLYPH $DRIVERS glyphmain; do
    clang++ -c $CXXFLAGS -o "$BUILD/$u.o" "native/stage/$u.cpp"
done

# posedump links avbdump.o for the OracleAvb* observability sinks: avbfile.cpp calls
# them under ORACLE_HARNESS whether or not this binary dumps assets.
OBJS=$(for u in $ENGINE $SHARED $GLYPH; do printf '%s ' "$BUILD/$u.o"; done)

clang++ -o "$BUILD/avbdump"  "$BUILD/avbmain.o"  $OBJS native/shim/msvcrand.cpp -lz
clang++ -o "$BUILD/posedump" "$BUILD/posemain.o" $OBJS native/shim/msvcrand.cpp -lz

# --- text measurement, before the dumps: if the frozen glyph table or the CDC
# wiring is wrong, everything downstream of line breaking is wrong too, and the
# per-check output here says which part rather than leaving a golden diff to
# interpret. Its own binary because it needs no engine objects.
clang++ -o "$BUILD/glyphcheck" \
    "$BUILD/glyphmain.o" "$BUILD/glyphtable.o" "$BUILD/glyphtable_cdc.o" \
    "$BUILD/ojson.o" native/shim/msvcrand.cpp
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

echo ""
echo "=== native vs frozen Windows goldens ==="
echo "matched   : $match"
echo "mismatched: $mismatch$mislist"

if [ "$mismatch" -ne 0 ]; then
    first=$(echo "$mislist" | awk '{print $1}')
    echo ""
    echo "first difference ($first):"
    diff "oracle/avb/$first.golden.json" "$OUT/$first.json" | head -20 || true
    exit 1
fi
echo ""
echo "Native macOS build reproduces all $match Windows goldens byte-for-byte"
echo "(33 asset manifests + avatario + avatar-pose)."
