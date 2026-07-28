// avbdump.cpp - Tier-2 .avb/.bgb manifest dump. See avbdump.h for why this is a
// separate translation unit rather than part of oracleharness.cpp.
//
// Builds on Windows (MSVC + MFC) and on macOS (clang + native/shim). The only
// platform-conditional code is the SEH layer: __try/__except exists on MSVC only,
// so on clang the MFC-exception guard runs alone and a decoder fault crashes
// rather than degrading to a loadStatus field.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>

#include "bbox.h"
#include "pe.h"
#include "dib.h"
#include "avbfile.h"
#include "avatar.h"
#include "backdrop.h"

#include "ojson.h"
#include "avbdump.h"

// ---------------------------------------------------------------------------
// Dump an {emotion, intensity} pair. FACEREC/BODYREC/RBODYREC store these as
// floats; widened to double so ojson emits at %.17g, matching how the textpose and
// avatario dumps express their emotion floats. (Duplicated from oracleharness.cpp
// deliberately - this file must not depend on the rest of the harness, since the
// native build links only this.)
// ---------------------------------------------------------------------------
static ojson::Value DumpEmotionPair(float emotion, float intensity) {
    ojson::Value v = ojson::Value::Obj();
    v.Set("emotion", ojson::Value::Dbl((double)emotion));
    v.Set("intensity", ojson::Value::Dbl((double)intensity));
    return v;
}

// ===========================================================================
// Tier-2: .avb / .bgb asset manifests + decoded-pixel CRC32s
//
// The unit under test is avbfile.cpp + dib.cpp: the record-tag walk that turns
// a file into a CAvatarX (name/flags/URLs, the global palette, and the
// face/torso/body record tables), and the image decode that turns a stream
// offset into a CAvatarDIB. Two layers, deliberately dumped side by side:
//
//   * pre-load facts   - per pose, the three (offset, format, paletteType)
//                        triples straight out of the pose records. No decode
//                        involved, so these stay meaningful even when an image
//                        fails to decode.
//   * post-load facts  - per image slot, the BITMAPINFOHEADER scalars plus a
//                        CRC32 over the pixel bytes.
//
// One golden per asset file (oracle/avb/<name>.golden.json) rather than one
// monolith: 32 assets worth of poses in a single file would be a multi-megabyte
// golden whose diff names nothing useful. Sharded, a mismatch names the asset.
// ===========================================================================

// Standard IEEE 802.3 CRC32 (reflected, init/final 0xFFFFFFFF), table built on
// first use. Chosen over a rolling sum because the TS port has crc32 available
// from any number of libraries and the same polynomial is unambiguous; a
// checksum that only this harness can compute would not be portable.
static unsigned long g_crcTable[256];
static bool g_crcTableReady = false;

static void CrcInit() {
    if (g_crcTableReady) return;
    for (unsigned long n = 0; n < 256; n++) {
        unsigned long c = n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        }
        g_crcTable[n] = c;
    }
    g_crcTableReady = true;
}

// Streaming form, so a hash can be accumulated across non-contiguous regions
// (needed for the row-by-row pixel hash below, which must skip row padding).
static unsigned long Crc32Begin() { CrcInit(); return 0xFFFFFFFFUL; }

static unsigned long Crc32Update(unsigned long c, const void* data, size_t len) {
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < len; i++) {
        c = g_crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    }
    return c;
}

static unsigned long Crc32End(unsigned long c) { return c ^ 0xFFFFFFFFUL; }

static unsigned long Crc32(const void* data, size_t len) {
    return Crc32End(Crc32Update(Crc32Begin(), data, len));
}

// CRCs go out as "0x........" strings, not numbers: ojson emits integers with
// %ld, so any CRC with the top bit set would land in the golden as a negative
// number. A fixed-width lowercase hex string is unambiguous in both directions.
static ojson::Value CrcStr(unsigned long crc) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08lx", crc & 0xFFFFFFFFUL);
    return ojson::Value::Str(buf);
}

// A char* field that is legitimately absent (no AK_COPYRIGHT record, say) is
// dumped as JSON null rather than "" - the port needs to tell "record missing"
// from "record present and empty".
static ojson::Value StrOrNull(const char* s) {
    return s ? ojson::Value::Str(s) : ojson::Value::Null();
}

// ---------------------------------------------------------------------------
// Tier-2 observability sinks. avbfile.cpp declares these extern under
// ORACLE_HARNESS and calls them at four points; see the comment block there for
// why each is needed. They answer coverage questions the loaded object graph
// cannot: the AK_* tag stream (the manifest pins tag RESULTS, not which tags
// exist), which image reader ran, and what biCompression an image had before
// ConvertToNonRLE erased it.
//
// Deliberately append-only counters and lists rather than anything the engine
// could read back: a hook that fed information INTO the engine would change what
// is being measured.
// ---------------------------------------------------------------------------
struct AvbTagRec { int tag; int size; };
static std::vector<AvbTagRec> g_avbTags;
static std::vector<std::pair<int, int> > g_avbImageReads;   // (format, paletteType)
static std::vector<std::pair<int, unsigned long> > g_avbPreConvert; // (compression, sizeImage)
static bool g_avbBackdropSeen = false;
static unsigned long g_avbBackdropOffset = 0;
static int g_avbBackdropFormat = -1;
static int g_avbBackdropPaletteType = -1;

static void OracleAvbTraceReset() {
    g_avbTags.clear();
    g_avbImageReads.clear();
    g_avbPreConvert.clear();
    g_avbBackdropSeen = false;
    g_avbBackdropOffset = 0;
    g_avbBackdropFormat = -1;
    g_avbBackdropPaletteType = -1;
}

void OracleAvbTag(int tag, int size) {
    AvbTagRec r; r.tag = tag; r.size = size;
    g_avbTags.push_back(r);
}

void OracleAvbImageRead(int slot, int format, int paletteType) {
    (void)slot;   // the slot is already in the manifest's per-pose records
    g_avbImageReads.push_back(std::make_pair(format, paletteType));
}

void OracleAvbPreConvert(int biCompression, unsigned long biSizeImage, int biBitCount) {
    (void)biBitCount;
    g_avbPreConvert.push_back(std::make_pair(biCompression, biSizeImage));
}

void OracleAvbBackdropRecord(unsigned long offset, int format, int paletteType) {
    g_avbBackdropSeen = true;
    g_avbBackdropOffset = offset;
    g_avbBackdropFormat = format;
    g_avbBackdropPaletteType = paletteType;
}

// Histogram helper: emits [{value, count}, ...] sorted by value. Sorted rather
// than insertion-ordered so the golden does not encode load order.
static ojson::Value DumpIntHistogram(const std::vector<int>& values) {
    std::vector<int> sorted(values);
    std::sort(sorted.begin(), sorted.end());
    ojson::Value arr = ojson::Value::Arr();
    for (size_t i = 0; i < sorted.size();) {
        size_t j = i;
        while (j < sorted.size() && sorted[j] == sorted[i]) j++;
        ojson::Value e = ojson::Value::Obj();
        e.Set("value", ojson::Value::Int((long)sorted[i]));
        e.Set("count", ojson::Value::Int((long)(j - i)));
        arr.Push(e);
        i = j;
    }
    return arr;
}

// The recorded tag stream, plus the histograms drained from the trace.
static void SetAvbTraceFields(ojson::Value& v) {
    ojson::Value tags = ojson::Value::Arr();
    for (size_t i = 0; i < g_avbTags.size(); i++) {
        ojson::Value e = ojson::Value::Obj();
        e.Set("tag", ojson::Value::Int((long)g_avbTags[i].tag));
        // -1 means the record carried no size field, i.e. an old (< 256) tag.
        e.Set("size", ojson::Value::Int((long)g_avbTags[i].size));
        tags.Push(e);
    }
    v.Set("recordTags", tags);

    std::vector<int> fmts, pals, comps;
    for (size_t i = 0; i < g_avbImageReads.size(); i++) {
        fmts.push_back(g_avbImageReads[i].first);
        pals.push_back(g_avbImageReads[i].second);
    }
    for (size_t i = 0; i < g_avbPreConvert.size(); i++)
        comps.push_back(g_avbPreConvert[i].first);

    v.Set("imageReadFormats", DumpIntHistogram(fmts));
    v.Set("imageReadPaletteTypes", DumpIntHistogram(pals));
    // Empty means CAvatarDIB::Load never ran for this asset, so ConvertToNonRLE
    // never ran either - which is itself the finding for an all-AIF_LZDEFLATE
    // corpus, since CAvatarFileZlibImage::Read does not go through that path.
    v.Set("preConvertCompressions", DumpIntHistogram(comps));
}

// ---------------------------------------------------------------------------
// Exception guards for the three asset-loading entry points.
//
// Each needs TWO layers, in two separate functions, for reasons that are all
// MSVC constraints rather than preference:
//
//   * The load can raise either kind of failure. Malformed image data faults
//     (an access violation, which is SEH); the stream reads underneath raise
//     MFC CFileException (which is a C++ throw). __except does not catch C++
//     exceptions under /EHsc, and CATCH_ALL does not catch faults - so both
//     guards are needed, and neither subsumes the other.
//   * MSVC forbids __try and C++ try/catch in the same function, so the two
//     guards cannot be nested inline; they nest across a function boundary.
//   * MSVC also rejects __try in any function holding a local that requires
//     unwinding (C2712), which is why these are standalone helpers instead of
//     living in the Capture* functions - those all hold ojson::Value locals.
//     LoadAvatarNoThrow above exists for the same reason.
//
// Status codes are shared by all three: 1 = ok, 0 = returned failure,
// -1 = MFC exception, -2 = access violation. Distinguishing the last two is
// worth the extra code: "the file is truncated" and "the decoder walked off
// the end of a buffer" are different bugs.
// ---------------------------------------------------------------------------
#define ORACLE_LOAD_OK        1
#define ORACLE_LOAD_FAILED    0
#define ORACLE_LOAD_MFC_EXC (-1)
#define ORACLE_LOAD_FAULT   (-2)

static int PoseLoadMfcGuard(CPose* pose, CAvatarStream* stream, CAvatarPalette* pal) {
    int r;
    TRY {
        r = pose->Load(stream, pal) ? ORACLE_LOAD_OK : ORACLE_LOAD_FAILED;
    }
    CATCH_ALL(e) {
        r = ORACLE_LOAD_MFC_EXC;
    }
    END_CATCH_ALL
    return r;
}

static int PoseLoadNoThrow(CPose* pose, CAvatarStream* stream, CAvatarPalette* pal) {
#ifdef _MSC_VER
    int r;
    __try {
        r = PoseLoadMfcGuard(pose, stream, pal);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        r = ORACLE_LOAD_FAULT;
    }
    return r;
#else
    // No SEH on clang/macOS. The MFC-exception layer still runs; an access
    // violation becomes a real crash instead of a loadStatus field. That is the
    // honest behaviour rather than a silent difference - and if it ever fires, the
    // Windows harness is the place to diagnose which pose did it.
    return PoseLoadMfcGuard(pose, stream, pal);
#endif
}

static CAvatarX* LoadAvbMfcGuard(CAvatarStream* stream, int* status) {
    CAvatarX* av = NULL;
    TRY {
        av = CAvatarX::LoadAvatar(stream);
        *status = av ? ORACLE_LOAD_OK : ORACLE_LOAD_FAILED;
    }
    CATCH_ALL(e) {
        av = NULL;
        *status = ORACLE_LOAD_MFC_EXC;
    }
    END_CATCH_ALL
    return av;
}

static CAvatarX* LoadAvbNoThrow(CAvatarStream* stream, int* status) {
#ifdef _MSC_VER
    CAvatarX* av = NULL;
    __try {
        av = LoadAvbMfcGuard(stream, status);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        av = NULL;
        *status = ORACLE_LOAD_FAULT;
    }
    return av;
#else
    return LoadAvbMfcGuard(stream, status);   // see PoseLoadNoThrow on SEH
#endif
}

static CChatBackdrop* LoadBgbMfcGuard(CAvatarStream* stream, int* status) {
    CChatBackdrop* bd = NULL;
    TRY {
        bd = CChatBackdrop::LoadBackdrop(stream);
        *status = bd ? ORACLE_LOAD_OK : ORACLE_LOAD_FAILED;
    }
    CATCH_ALL(e) {
        bd = NULL;
        *status = ORACLE_LOAD_MFC_EXC;
    }
    END_CATCH_ALL
    return bd;
}

static CChatBackdrop* LoadBgbNoThrow(CAvatarStream* stream, int* status) {
#ifdef _MSC_VER
    CChatBackdrop* bd = NULL;
    __try {
        bd = LoadBgbMfcGuard(stream, status);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        bd = NULL;
        *status = ORACLE_LOAD_FAULT;
    }
    return bd;
#else
    return LoadBgbMfcGuard(stream, status);   // see PoseLoadNoThrow on SEH
#endif
}

// ---------------------------------------------------------------------------
// Is a record-table count safe to iterate?
//
// This guard exists because the record tables are NOT initialised on
// construction. CAvatarX::Initialize (avatar.cpp:886) covers only base-class
// members; CAvatarComplex() sets just m_lastFace/m_lastTorso and CAvatarSimple()
// just m_lastBody, so fRec/bRec/nFaces/nTorsos/m_nBodies are indeterminate until
// LoadFaceRecs/LoadTorsoRecs/LoadBodyRecs writes the pointer and the count as a
// pair (avbfile.cpp:1077, 1181, 1255). An .avb missing its AK_NFACES/AK_NTORSOS/
// AK_NBODIES record therefore leaves BOTH garbage - so a NULL check on the
// pointer is not enough, and iterating "count" entries would read out of bounds.
//
// Every shipped asset has the records, so this should never fire; it is here so
// that if it ever does, the dump says so instead of hashing stack garbage.
//
// PORT NOTE: the TS side must zero-initialise these fields rather than mirror
// the C++ constructors, or a malformed .avb turns into an out-of-bounds read.
// ---------------------------------------------------------------------------
#define ORACLE_MAX_PLAUSIBLE_RECS 4096

static bool RecCountPlausible(const void* recs, long count) {
    return recs != NULL && count >= 0 && count <= ORACLE_MAX_PLAUSIBLE_RECS;
}

// Status code -> golden field. Emitted as a name rather than the number so the
// golden stays readable and a new code cannot silently look like an old one.
static ojson::Value LoadStatusStr(int status) {
    switch (status) {
        case ORACLE_LOAD_OK:      return ojson::Value::Str("ok");
        case ORACLE_LOAD_FAILED:  return ojson::Value::Str("failed");
        case ORACLE_LOAD_MFC_EXC: return ojson::Value::Str("mfcException");
        case ORACLE_LOAD_FAULT:   return ojson::Value::Str("accessViolation");
        default:                  return ojson::Value::Str("unknown");
    }
}

// ---------------------------------------------------------------------------
// Dump a CAvatarPalette.
//
// Entries are listed in full up to 256 and carried by the CRC alone beyond that.
// avbfile.h allows MAX_PALETTE_SIZE = 2048, and 2048 lines per asset across 25
// assets would dominate the golden without adding a failure mode the CRC misses.
// The threshold is generous enough that every shipped 8bpp palette lists in full.
//
// COLORREF is 0x00BBGGRR on Win32 - byte 0 is RED, not blue. Emitted as hex so
// that byte order is visible in the golden rather than hidden in a decimal, and
// so a port that swaps the channels is obvious on sight.
// ---------------------------------------------------------------------------
#define ORACLE_MAX_LISTED_COLORS 256

static ojson::Value DumpAvatarPalette(CAvatarPalette& pal) {
    ojson::Value v = ojson::Value::Obj();
    v.Set("colorCount", ojson::Value::Int((long)pal.m_nColorCount));
    if (pal.m_pclrref == NULL) {
        v.Set("crc32", ojson::Value::Null());
        v.Set("colors", ojson::Value::Null());
        return v;
    }
    v.Set("crc32", CrcStr(Crc32(pal.m_pclrref, pal.m_nColorCount * sizeof(COLORREF))));
    if (pal.m_nColorCount > ORACLE_MAX_LISTED_COLORS) {
        v.Set("colors", ojson::Value::Str("elided (see crc32)"));
        return v;
    }
    ojson::Value colors = ojson::Value::Arr();
    for (UINT i = 0; i < pal.m_nColorCount; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "0x%06lx", (unsigned long)(pal.m_pclrref[i] & 0xFFFFFFUL));
        colors.Push(ojson::Value::Str(buf));
    }
    v.Set("colors", colors);
    return v;
}

// ---------------------------------------------------------------------------
// Dump one decoded image slot.
//
// Byte extent: for BI_RGB it is StorageWidth() * abs(biHeight) - biSizeImage is
// documented as "may be 0" for uncompressed DIBs and dib.cpp does set it to 0
// on the paths that build headers by hand (dib.cpp:295, 377), so it cannot be
// trusted as the length. For the RLE compressions biSizeImage IS the length and
// the stride formula does not apply. Both cases are recorded so a port that
// picks the wrong one shows up as a length mismatch, not a silent CRC drift.
//
// DIBStorageWidth's own quirk is worth noting for the port: for bpp < 8 it
// rounds nWidth UP by (8/bpp - 1) before the multiply (dib.cpp:1027), which is
// arithmetically the same as ceil(width*bpp/8) but reads differently enough
// that a transliteration is easy to get wrong. storageWidth is dumped so the
// port checks the result rather than the derivation.
// ---------------------------------------------------------------------------
static ojson::Value DumpDibSlot(CAvatarDIB* dib) {
    if (dib == NULL) return ojson::Value::Null();

    ojson::Value v = ojson::Value::Obj();
    BITMAPINFO* bmi = dib->GetBitmapInfoAddress();
    if (bmi == NULL) {
        v.Set("bitmapInfo", ojson::Value::Null());
        return v;
    }
    const BITMAPINFOHEADER& h = bmi->bmiHeader;
    v.Set("biSize", ojson::Value::Int((long)h.biSize));
    v.Set("biWidth", ojson::Value::Int((long)h.biWidth));
    v.Set("biHeight", ojson::Value::Int((long)h.biHeight));
    v.Set("biPlanes", ojson::Value::Int((long)h.biPlanes));
    v.Set("biBitCount", ojson::Value::Int((long)h.biBitCount));
    v.Set("biCompression", ojson::Value::Int((long)h.biCompression));
    v.Set("biSizeImage", ojson::Value::Int((long)h.biSizeImage));
    v.Set("biClrUsed", ojson::Value::Int((long)h.biClrUsed));
    v.Set("biClrImportant", ojson::Value::Int((long)h.biClrImportant));

    // The accessors, not the header, so the golden pins what callers actually
    // see (GetWidth/GetHeight are virtual and delegate to DibWidth/DibHeight).
    v.Set("getWidth", ojson::Value::Int((long)dib->GetWidth()));
    v.Set("getHeight", ojson::Value::Int((long)dib->GetHeight()));
    long stride = (long)dib->StorageWidth();
    v.Set("storageWidth", ojson::Value::Int(stride));

    // NumDIBColorEntries (the free function, dib.cpp:70) rather than the
    // CDIB::GetNumClrEntries method it wraps. The method is declared in dib.h:30
    // - outside that header's own #if 0 - but its definition sits INSIDE a
    // 546-line #if 0 in dib.cpp (260-806), together with Create(int,int), three
    // Load overloads, MapColorsToPalette, GetPixelAddress, GetRect and CopyBits.
    // So it is a declaration with no definition: calling it links fine right up
    // until someone actually does, which is how this first surfaced (LNK2019).
    int nClr = NumDIBColorEntries(bmi);
    v.Set("numClrEntries", ojson::Value::Int((long)nClr));
    if (nClr > 0) {
        RGBQUAD* tab = dib->GetClrTabAddress();
        v.Set("clrTabCrc32", CrcStr(Crc32(tab, (size_t)nClr * sizeof(RGBQUAD))));
    } else {
        v.Set("clrTabCrc32", ojson::Value::Null());
    }

    void* bits = dib->GetBitsAddress();
    if (bits == NULL) {
        v.Set("allocBytes", ojson::Value::Null());
        v.Set("pixelRowBytes", ojson::Value::Null());
        v.Set("pixelCrc32", ojson::Value::Null());
        return v;
    }
    long absHeight = h.biHeight < 0 ? -(long)h.biHeight : (long)h.biHeight;

    if (h.biCompression != BI_RGB) {
        // No shipped asset reaches here - CAvatarDIB::Load expands RLE before
        // any caller sees the DIB (avbfile.cpp:462) - but if one ever does,
        // biSizeImage is the length and there are no rows to walk.
        size_t len = (size_t)h.biSizeImage;
        v.Set("allocBytes", ojson::Value::Int((long)len));
        v.Set("pixelRowBytes", ojson::Value::Null());
        v.Set("pixelCrc32", len ? CrcStr(Crc32(bits, len)) : ojson::Value::Null());
        return v;
    }

    // Hash the PIXELS, not the buffer. Row padding is deliberately excluded, for
    // two independent reasons:
    //
    // 1. It is not reproducible. CPose::ConvertMasksCommon writes only
    //    srcStride/2 bytes per destination row but strides by the full
    //    destination stride (avbfile.cpp:1625-1655), so whenever
    //    srcStride/2 < destStride the tail of every scan line is never written -
    //    and the ZeroMemory that would cover it sits behind
    //    #ifdef AVATAR_NOT_CLIENT (avbfile.cpp:1585-1587), which the client build
    //    does not define. Those bytes are uninitialised malloc memory, so a
    //    full-stride CRC changes from run to run. Measured: 1-bpp slots with
    //    biWidth % 32 in 1..16 moved between two CI runs; % 32 == 0 and 17..31
    //    were stable, exactly as the two stride formulas predict.
    // 2. It is not portable even if it were stable. A port is free to store rows
    //    unpadded, or to zero its padding; either would be correct and both would
    //    fail a full-stride comparison.
    //
    // So: per row, hash ceil(width * bpp / 8) bytes and mask the final byte down
    // to exactly biWidth pixels. DIBs are MSB-first within a byte, so the valid
    // bits are the HIGH ones - hence 0xFF << (8 - tailBits).
    long bitsPerRow = (long)h.biWidth * (long)h.biBitCount;
    long rowBytes = (bitsPerRow + 7) / 8;
    v.Set("allocBytes", ojson::Value::Int(stride * absHeight));
    v.Set("pixelRowBytes", ojson::Value::Int(rowBytes));
    if (rowBytes <= 0 || absHeight <= 0) {
        v.Set("pixelCrc32", ojson::Value::Null());
        return v;
    }
    int tailBits = (int)(bitsPerRow % 8);
    unsigned char tailMask = tailBits ? (unsigned char)(0xFF << (8 - tailBits))
                                      : (unsigned char)0xFF;
    unsigned long c = Crc32Begin();
    for (long y = 0; y < absHeight; y++) {
        const unsigned char* row = (const unsigned char*)bits + (size_t)y * (size_t)stride;
        if (rowBytes > 1) c = Crc32Update(c, row, (size_t)(rowBytes - 1));
        unsigned char last = (unsigned char)(row[rowBytes - 1] & tailMask);
        c = Crc32Update(c, &last, 1);
    }
    v.Set("pixelCrc32", CrcStr(Crc32End(c)));
    return v;
}

// ---------------------------------------------------------------------------
// Dump one pose: the pre-load record triples, then the three decoded slots.
//
// Poses load lazily - CAvatarX::GetPoseFromID (avatar.cpp:135) checks
// m_pdibs[0] and calls CPose::Load only on a miss. GetPoseFromID is protected
// on CAvatarX and its public overloads on the derived classes are keyed by
// poseID with fallback search, which would silently substitute a different pose
// on a miss. So this walks m_arrPoses by index and calls Load itself: index i
// IS poseID i+1 (that same m_arrPoses[nID - 1] indexing), and a failed load is
// then reported as a failure rather than papered over.
// ---------------------------------------------------------------------------
static const char* kSlotNames[3] = { "drawing", "mask", "aura" };

static ojson::Value DumpPose(CPose* pose, int index, CAvatarStream* stream,
                             CAvatarPalette* pal) {
    ojson::Value v = ojson::Value::Obj();
    v.Set("index", ojson::Value::Int(index));
    v.Set("poseID", ojson::Value::Int(index + 1));

    // Pre-load: the record as it appears in the file.
    ojson::Value recs = ojson::Value::Arr();
    for (int i = 0; i < 3; i++) {
        ojson::Value r = ojson::Value::Obj();
        r.Set("slot", ojson::Value::Str(kSlotNames[i]));
        r.Set("offset", ojson::Value::Int((long)pose->m_dwOffsets[i]));
        r.Set("format", ojson::Value::Int((long)pose->m_byFormats[i]));
        r.Set("paletteType", ojson::Value::Int((long)pose->m_byPaletteTypes[i]));
        recs.Push(r);
    }
    v.Set("records", recs);

    // Post-load. A decode that faults must not take the whole dump with it:
    // the pre-load half above is still worth freezing, and knowing WHICH pose
    // faulted is the diagnosis. (--codecs cost two CI rounds to learn this.)
    int status;
    if (pose->m_pdibs[0] != NULL) {
        status = ORACLE_LOAD_OK;   // already resident
    } else if (stream != NULL) {
        status = PoseLoadNoThrow(pose, stream, pal);
    } else {
        status = ORACLE_LOAD_FAILED;
    }
    bool loaded = (status == ORACLE_LOAD_OK);
    v.Set("loadStatus", LoadStatusStr(status));

    ojson::Value slots = ojson::Value::Arr();
    for (int i = 0; i < 3; i++) {
        ojson::Value s = ojson::Value::Obj();
        s.Set("slot", ojson::Value::Str(kSlotNames[i]));
        s.Set("dib", loaded ? DumpDibSlot(pose->m_pdibs[i]) : ojson::Value::Null());
        slots.Push(s);
    }
    v.Set("slots", slots);
    return v;
}

// ---------------------------------------------------------------------------
// Read a whole file and CRC it. Pins the exact asset bytes the manifest was
// derived from, so a port comparing manifests knows it read the same file
// rather than a differently-versioned ComicArt.
// ---------------------------------------------------------------------------
static ojson::Value DumpFileIdentity(const char* path) {
    ojson::Value v = ojson::Value::Obj();
    FILE* f = fopen(path, "rb");
    if (!f) {
        v.Set("bytes", ojson::Value::Null());
        v.Set("crc32", ojson::Value::Null());
        return v;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf((size_t)(sz > 0 ? sz : 0), '\0');
    if (sz > 0) fread(&buf[0], 1, (size_t)sz, f);
    fclose(f);
    v.Set("bytes", ojson::Value::Int(sz));
    v.Set("crc32", CrcStr(Crc32(buf.data(), buf.size())));
    return v;
}

// ---------------------------------------------------------------------------
// Manifest one .avb file.
//
// Loads through CAvatarX::LoadAvatar directly rather than the engine's
// LoadAvatar(name)/LoadAvatarInfo pair. Two reasons: LoadAvatarInfo derives its
// path from theApp.GetAvatarDir() and calls SetNewName, which overwrites m_name
// with the FILENAME - exactly the field a format manifest wants to pin from the
// AK_NAME record. Going straight at the file also keeps this dump on avbfile.cpp
// (the unit under test) instead of the avatar registry.
// ---------------------------------------------------------------------------
static ojson::Value CaptureAvbFile(const char* path, const char* basename) {
    ojson::Value v = ojson::Value::Obj();
    v.Set("file", ojson::Value::Str(basename));
    v.Set("kind", ojson::Value::Str("avatar"));
    v.Set("identity", DumpFileIdentity(path));

    fprintf(stderr, "ORACLE: avb '%s'\n", basename);
    fflush(stderr);

    OracleAvbTraceReset();
    CAvatarFileStream* stream = new CAvatarFileStream(path);
    int status = ORACLE_LOAD_FAILED;
    CAvatarX* av = LoadAvbNoThrow(stream, &status);
    v.Set("loadStatus", LoadStatusStr(status));
    if (av == NULL) {
        SetAvbTraceFields(v);   // the tag stream is the diagnosis for a failed load
        delete stream;
        return v;
    }
    av->SetStream(stream);

    // Header-ish scalars. m_avatarID is a registry-assigned runtime value, not
    // a file fact, so it is deliberately NOT dumped - it would make the golden
    // depend on load order.
    v.Set("name", StrOrNull(av->m_name));
    v.Set("style", ojson::Value::Int((long)av->m_style));
    v.Set("flags", ojson::Value::Int((long)av->m_flags));
    v.Set("freeze", ojson::Value::Int((long)av->m_freeze));
    v.Set("icon", ojson::Value::Int((long)av->m_icon));
    v.Set("originalURL", StrOrNull(av->m_pszOriginalURL));
    v.Set("newURL", StrOrNull(av->m_pszNewURL));
    v.Set("copyright", StrOrNull(av->m_pszCopyright));
    v.Set("poseCount", ojson::Value::Int((long)av->GetPoseCount()));
    v.Set("palette", DumpAvatarPalette(av->m_palette));

    // The record tables. dynamic_cast rather than a flag test: the two classes
    // carry genuinely different tables (FACEREC+BODYREC vs RBODYREC), and the
    // AT_SIMPLE/AT_COMPLEX type byte is consumed inside LoadAvatar and not kept.
    CAvatarComplex* cx = dynamic_cast<CAvatarComplex*>(av);
    CAvatarSimple* sx = dynamic_cast<CAvatarSimple*>(av);
    if (cx != NULL) {
        v.Set("class", ojson::Value::Str("complex"));
        bool faceOk = RecCountPlausible(cx->fRec, (long)cx->nFaces);
        bool torsoOk = RecCountPlausible(cx->bRec, (long)cx->nTorsos);
        v.Set("nFaces", faceOk ? ojson::Value::Int((long)cx->nFaces) : ojson::Value::Null());
        v.Set("nTorsos", torsoOk ? ojson::Value::Int((long)cx->nTorsos) : ojson::Value::Null());
        if (!faceOk) v.Set("faceRecsUnreadable", ojson::Value::Bool(true));
        if (!torsoOk) v.Set("torsoRecsUnreadable", ojson::Value::Bool(true));
        ojson::Value faces = ojson::Value::Arr();
        for (int i = 0; faceOk && i < cx->nFaces; i++) {
            const FACEREC& r = cx->fRec[i];
            ojson::Value e = ojson::Value::Obj();
            e.Set("index", ojson::Value::Int(i));
            e.Set("poseID", ojson::Value::Int((long)r.poseID));
            e.Set("emotionPair", DumpEmotionPair(r.emotion, r.intensity));
            e.Set("xCX", ojson::Value::Int((long)r.xCX));
            e.Set("yCX", ojson::Value::Int((long)r.yCX));
            e.Set("delta_xCX", ojson::Value::Int((long)r.delta_xCX));
            e.Set("delta_yCX", ojson::Value::Int((long)r.delta_yCX));
            e.Set("faceX", ojson::Value::Int((long)r.faceX));
            e.Set("faceY", ojson::Value::Int((long)r.faceY));
            faces.Push(e);
        }
        v.Set("faceRecs", faces);
        ojson::Value torsos = ojson::Value::Arr();
        for (int i = 0; torsoOk && i < cx->nTorsos; i++) {
            const BODYREC& r = cx->bRec[i];
            ojson::Value e = ojson::Value::Obj();
            e.Set("index", ojson::Value::Int(i));
            e.Set("poseID", ojson::Value::Int((long)r.poseID));
            e.Set("emotionPair", DumpEmotionPair(r.emotion, r.intensity));
            e.Set("xCX", ojson::Value::Int((long)r.xCX));
            e.Set("yCX", ojson::Value::Int((long)r.yCX));
            torsos.Push(e);
        }
        v.Set("torsoRecs", torsos);
    } else if (sx != NULL) {
        v.Set("class", ojson::Value::Str("simple"));
        bool bodyOk = RecCountPlausible(sx->bRec, (long)sx->m_nBodies);
        v.Set("nBodies", bodyOk ? ojson::Value::Int((long)sx->m_nBodies) : ojson::Value::Null());
        if (!bodyOk) v.Set("bodyRecsUnreadable", ojson::Value::Bool(true));
        ojson::Value bodies = ojson::Value::Arr();
        for (int i = 0; bodyOk && i < sx->m_nBodies; i++) {
            const RBODYREC& r = sx->bRec[i];
            ojson::Value e = ojson::Value::Obj();
            e.Set("index", ojson::Value::Int(i));
            e.Set("poseID", ojson::Value::Int((long)r.poseID));
            e.Set("emotionPair", DumpEmotionPair(r.emotion, r.intensity));
            e.Set("faceX", ojson::Value::Int((long)r.faceX));
            e.Set("faceY", ojson::Value::Int((long)r.faceY));
            bodies.Push(e);
        }
        v.Set("bodyRecs", bodies);
    } else {
        v.Set("class", ojson::Value::Str("unknown"));
    }

    ojson::Value poses = ojson::Value::Arr();
    int n = av->GetPoseCount();
    for (int i = 0; i < n; i++) {
        fprintf(stderr, "ORACLE:   pose %d/%d\n", i + 1, n);
        fflush(stderr);
        poses.Push(DumpPose(av->m_arrPoses[i], i, stream, &av->m_palette));
    }
    v.Set("poses", poses);

    // Drained after the poses, so imageReadFormats/preConvertCompressions cover
    // every image this asset decoded rather than only the tag-walk phase.
    SetAvbTraceFields(v);

    // The avatar owns the stream once SetStream is called; deleting the avatar
    // does NOT free it (CAvatarX::~CAvatarX leaves m_pStream alone), so the
    // stream is leaked deliberately - this is a one-shot console dump and
    // freeing it after the DIBs were built off it is not worth the risk.
    return v;
}

// ---------------------------------------------------------------------------
// Manifest one .bgb file. A backdrop is a single image plus URL/copyright -
// CChatBackdrop::Load (avbfile.cpp:1774) rejects anything but AT_BACKDROP with
// an AK_BACKDROP record whose image is AIP_LOCALPALETTE or AIP_NOPALETTE, so
// there is no global palette and no pose table to dump.
// ---------------------------------------------------------------------------
static ojson::Value CaptureBgbFile(const char* path, const char* basename) {
    ojson::Value v = ojson::Value::Obj();
    v.Set("file", ojson::Value::Str(basename));
    v.Set("kind", ojson::Value::Str("backdrop"));
    v.Set("identity", DumpFileIdentity(path));

    fprintf(stderr, "ORACLE: bgb '%s'\n", basename);
    fflush(stderr);

    OracleAvbTraceReset();
    CAvatarFileStream* stream = new CAvatarFileStream(path);
    int status = ORACLE_LOAD_FAILED;
    CChatBackdrop* bd = LoadBgbNoThrow(stream, &status);
    v.Set("loadStatus", LoadStatusStr(status));
    if (bd == NULL) {
        SetAvbTraceFields(v);
        delete stream;
        return v;
    }
    v.Set("originalURL", StrOrNull(bd->m_pszOrigURL));
    v.Set("newURL", StrOrNull(bd->m_pszNewURL));
    v.Set("copyright", StrOrNull(bd->m_pszCopyright));

    // The AK_BACKDROP record's own (offset, format, paletteType). These live in a
    // local inside CChatBackdrop::Load and are dropped on the floor - unlike an
    // avatar pose, the loaded CChatBackdrop cannot be asked - so they arrive via
    // the ORACLE_HARNESS hook. Without them a backdrop manifest could not say
    // whether the image came through the DIB reader or the zlib one.
    ojson::Value rec = ojson::Value::Obj();
    if (g_avbBackdropSeen) {
        rec.Set("offset", ojson::Value::Int((long)g_avbBackdropOffset));
        rec.Set("format", ojson::Value::Int((long)g_avbBackdropFormat));
        rec.Set("paletteType", ojson::Value::Int((long)g_avbBackdropPaletteType));
    }
    v.Set("record", g_avbBackdropSeen ? rec : ojson::Value::Null());

    v.Set("drawing", DumpDibSlot(bd->GetDrawing()));
    SetAvbTraceFields(v);
    return v;
}

// ---------------------------------------------------------------------------
// Enumerate ComicArt and manifest every asset. Returns the index; the per-asset
// manifests are written by the caller.
//
// FindFirstFile order is filesystem-dependent (it is NOT specified to be
// sorted), so names are collected then sorted explicitly. Without that, the
// index.json ordering would be a property of the runner's disk.
// ---------------------------------------------------------------------------
static void ListAssets(const char* artDir, const char* pattern,
                       std::vector<std::string>& out) {
    char glob[MAX_PATH];
    // Forward slash, not backslash: this file is compiled for BOTH Windows and
    // macOS. Win32 file APIs accept '/' as a separator, whereas macOS treats '\\'
    // as an ordinary filename character - so a backslash here silently produced
    // files literally named "avbout\\anna.json" on the native build. Nothing in the
    // dump output contains a separator, so the goldens are unaffected either way.
    snprintf(glob, MAX_PATH, "%s/%s", artDir, pattern);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(glob, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            out.push_back(std::string(fd.cFileName));
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
}

// Writes <outDir>\<basename>.json for each asset and returns the index value.
ojson::Value CaptureAvb(const char* artDir, const char* outDir) {
    // artDir deliberately does NOT go into the dump: it is an invocation
    // argument, not an observable, and putting it in would make the frozen
    // golden depend on the path the harness happened to be called with.
    ojson::Value root = ojson::Value::Obj();

    std::vector<std::string> avbs, bgbs;
    ListAssets(artDir, "*.avb", avbs);
    ListAssets(artDir, "*.bgb", bgbs);
    fprintf(stderr, "ORACLE: avb dump - %d avatars, %d backdrops in %s\n",
            (int)avbs.size(), (int)bgbs.size(), artDir);
    fflush(stderr);

    ojson::Value assets = ojson::Value::Arr();
    for (size_t pass = 0; pass < 2; pass++) {
        std::vector<std::string>& names = (pass == 0) ? avbs : bgbs;
        for (size_t i = 0; i < names.size(); i++) {
            const std::string& base = names[i];
            char path[MAX_PATH];
            snprintf(path, MAX_PATH, "%s/%s", artDir, base.c_str());

            ojson::Value m = (pass == 0) ? CaptureAvbFile(path, base.c_str())
                                         : CaptureBgbFile(path, base.c_str());
            std::string text = m.EmitToString();

            // Per-asset golden name: strip the extension, keep the stem.
            std::string stem = base;
            size_t dot = stem.rfind('.');
            if (dot != std::string::npos) stem = stem.substr(0, dot);
            char outPath[MAX_PATH];
            snprintf(outPath, MAX_PATH, "%s/%s.json", outDir, stem.c_str());
            FILE* f = fopen(outPath, "wb");
            if (!f) {
                fprintf(stderr, "ORACLE: cannot write %s\n", outPath);
                fflush(stderr);
                continue;
            }
            fwrite(text.c_str(), 1, text.size(), f);
            fclose(f);

            // The index carries a CRC of the manifest text itself, so a single
            // line in index.json tells you whether any asset moved at all.
            ojson::Value e = ojson::Value::Obj();
            e.Set("file", ojson::Value::Str(base));
            e.Set("manifest", ojson::Value::Str(stem + ".json"));
            e.Set("kind", ojson::Value::Str(pass == 0 ? "avatar" : "backdrop"));
            const ojson::Value* id = m.Find("identity");
            if (id) {
                const ojson::Value* crc = id->Find("crc32");
                const ojson::Value* bytes = id->Find("bytes");
                if (bytes) e.Set("bytes", *bytes);
                if (crc) e.Set("fileCrc32", *crc);
            }
            e.Set("manifestCrc32", CrcStr(Crc32(text.data(), text.size())));
            assets.Push(e);
        }
    }
    root.Set("avatarCount", ojson::Value::Int((long)avbs.size()));
    root.Set("backdropCount", ojson::Value::Int((long)bgbs.size()));
    root.Set("assets", assets);
    return root;
}
