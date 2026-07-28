#!/bin/sh
# verify-avb.sh - build the native Tier-2 dump and diff it against the frozen
# Windows goldens. Milestone 1 of the native port, and its regression test.
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
for c in avbmain nativeglue; do
    ln -sf "$ROOT/native/$c.cpp" "native/stage/$c.cpp"
done

mkdir -p "$BUILD"

# Engine objects the dump needs, plus the harness dump itself. Deliberately NOT
# userinfo.o: it compiles, but importing it drags in the history and protocol layer
# (AddAndExecute, GetMembers, the HistoryEntry vtables) for a dump that needs none
# of it - native/nativeglue.cpp stubs the one symbol it was wanted for.
UNITS="avbmain nativeglue avbdump ojson oracleseed avbfile dib avatar backdrop avatario vector2d bbox"

for u in $UNITS; do
    clang++ -c $CXXFLAGS -o "$BUILD/$u.o" "native/stage/$u.cpp"
done

clang++ -o "$BUILD/avbdump" \
    $(for u in $UNITS; do printf '%s ' "$BUILD/$u.o"; done) \
    native/shim/msvcrand.cpp -lz

rm -rf "$OUT"
mkdir -p "$OUT"
"$BUILD/avbdump" "$OUT" v2.5-beta-1-modern/ComicArt 2> "$BUILD/avbdump.log"

match=0
mismatch=0
mislist=""
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
echo "Native macOS build reproduces all $match Windows goldens byte-for-byte."
