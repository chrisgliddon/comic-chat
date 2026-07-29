// cgsurface.cpp - offscreen bitmaps and memory DCs, over Core Graphics.
//
// The engine composites into an offscreen buffer and blits the result, which is how a
// 1996 Win32 app avoided flicker. CBodyCam::DrawBody (bodycam.cpp:487) is the case that
// forced this file:
//
//     CDC memDC;
//     VERIFY(memDC.CreateCompatibleDC(dc));
//     CBitmap *retCBit = temp.FromHandle(m_retSec);      // the retained DIB section
//     CBitmap *bmpOld  = memDC.SelectObject(retCBit);
//     memDC.FillSolidRect(&m_bodyRect, RGB(255, 255, 255));
//     brect = body->DrawBody(&memDC, rect2, FALSE);      // the character, into the buffer
//     VERIFY(dc->BitBlt(rect.left, oldTop, rect.right, rect.bottom, &memDC, 0, 0, SRCCOPY));
//
// With CreateCompatibleDC and SelectObject as no-ops returning TRUE, every one of those
// calls "succeeded" and the character silently never appeared - the memory DC had no
// surface, so CDC's entry points all took their `if (!m_cgCtx) return` path.
//
// WHAT IS AND IS NOT MODELLED. A Windows DIB section is two things at once: a GDI drawing
// surface and a block of memory the caller may read and write directly. Only the first is
// needed here, and that is not an assumption - it is checked: m_retDib, the CDIB wrapper
// around the section's bits, is allocated by CreateRetainedBitmap and freed by
// FreeRetainedPanel and never read in between (bodycam.cpp: only lines 108, 1093 and 1105
// mention it). So the bits are allocated to the real Windows layout, and made available,
// but drawing goes to a companion 32-bit CG context rather than being rasterised into them.
//
// If some path ever DOES read a section's bits after drawing into it, this is where it
// would go wrong, and it would need a flush from the CG context back into the DIB bytes.
// Named here so that is a known edge rather than a mystery.

#include "stdafx.h"
#include "render.h"          // ApplicationServices, and the MAX/MIN #undef

#include <map>
#include <vector>

namespace {

struct Surface {
    int w, h;
    CGContextRef ctx;           // 32-bit premultiplied RGBA, flipped so y runs downward
    std::vector<BYTE> dibBits;  // the Windows-layout bytes, for a DIB section; else empty
    Surface() : w(0), h(0), ctx(0) {}
};

// Keyed by the HBITMAP the engine passes around. Surfaces live for the process: the engine
// frees a retained section with ::DeleteObject, which the shim treats as a no-op, and
// reclaiming here would leave a dangling CGContext behind whichever DC still had it
// selected.
std::map<HBITMAP, Surface*> g_surfaces;

// CBitmap objects handed out by CBitmap::FromHandle. MFC keeps a per-thread temporary map
// and cleans it up at idle; one permanent entry per handle is the same observable
// behaviour for this build, without an idle hook to hang the cleanup on.
std::map<HBITMAP, CBitmap*> g_wrappers;

Surface* NewSurface(int w, int h) {
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;
    Surface* s = new Surface;
    s->w = w; s->h = h;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    s->ctx = CGBitmapContextCreate(NULL, w, h, 8, 0, cs, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    if (!s->ctx) { delete s; return 0; }

    // An offscreen GDI bitmap starts BLACK, but every caller here fills it before drawing
    // (DrawBody's FillSolidRect), and starting white makes a missed fill look like a missed
    // fill rather than like a missing character. Then the same y-downward flip a window
    // gets, because the engine addresses this surface in client coordinates.
    CGContextSetRGBFillColor(s->ctx, 1, 1, 1, 1);
    CGContextFillRect(s->ctx, CGRectMake(0, 0, w, h));
    CGContextTranslateCTM(s->ctx, 0, (CGFloat)h);
    CGContextScaleCTM(s->ctx, 1.0, -1.0);
    return s;
}

// A distinct, non-NULL HBITMAP per surface. The value is never dereferenced as a pointer by
// the engine - it is only compared against NULL and passed back to FromHandle - so a
// counter is enough, and it avoids handing out heap addresses that ::DeleteObject might
// plausibly be expected to free.
HBITMAP NextHandle() {
    static uintptr_t next = 0x8000;
    return (HBITMAP)(void*)(++next * 16);
}

Surface* SurfaceFor(HBITMAP h) {
    std::map<HBITMAP, Surface*>::iterator it = g_surfaces.find(h);
    return (it == g_surfaces.end()) ? 0 : it->second;
}

} // namespace

// --- creating surfaces -----------------------------------------------------

// The engine hands in a fully populated BITMAPINFOHEADER and expects a pointer to writable
// bits laid out the Windows way: DWORD-aligned rows, bottom-up unless biHeight is negative.
void* CreateDIBSection(HDC, const BITMAPINFO* bmi, UINT, void** ppvBits, HANDLE, DWORD) {
    if (ppvBits) *ppvBits = 0;
    if (!bmi) return 0;
    const BITMAPINFOHEADER& h = bmi->bmiHeader;
    int w = (int)h.biWidth;
    int hh = (int)(h.biHeight < 0 ? -h.biHeight : h.biHeight);
    int bpp = (int)h.biBitCount;
    if (w <= 0 || hh <= 0 || bpp <= 0) {
        // biBitCount 0 is the signature of a header that GetOptimalDibSectionInfo failed to
        // fill in - see the BITSPIXEL note in glyphtable_cdc.cpp. Worth naming, because the
        // engine ASSERTs on the result and an assert is a worse clue than this.
        fprintf(stderr, "cgsurface: CreateDIBSection with an unusable header "
                        "(%dx%d, %d bpp)\n", w, hh, bpp);
        return 0;
    }

    Surface* s = NewSurface(w, hh);
    if (!s) return 0;
    size_t stride = (size_t)(((w * bpp + 31) / 32) * 4);
    s->dibBits.assign(stride * (size_t)hh, 0);

    HBITMAP hb = NextHandle();
    g_surfaces[hb] = s;
    if (ppvBits) *ppvBits = &s->dibBits[0];
    return (void*)hb;
}

HBITMAP CreateCompatibleBitmap(HDC, int w, int h) {
    Surface* s = NewSurface(w, h);
    if (!s) return (HBITMAP)0;
    HBITMAP hb = NextHandle();
    g_surfaces[hb] = s;
    return hb;
}

BOOL CBitmap::CreateCompatibleBitmap(CDC*, int w, int h) {
    m_hBitmap = ::CreateCompatibleBitmap((HDC)0, w, h);
    return m_hBitmap != 0;
}

BOOL CBitmap::CreateBitmap(int w, int h, UINT, UINT, const void*) {
    m_hBitmap = ::CreateCompatibleBitmap((HDC)0, w, h);
    return m_hBitmap != 0;
}

CBitmap* CBitmap::FromHandle(HBITMAP h) {
    if (!h) return 0;
    std::map<HBITMAP, CBitmap*>::iterator it = g_wrappers.find(h);
    if (it != g_wrappers.end()) return it->second;
    CBitmap* b = new CBitmap;
    b->m_hBitmap = h;
    g_wrappers[h] = b;
    return b;
}

// --- memory DCs ------------------------------------------------------------

BOOL CDC::CreateCompatibleDC(CDC* pSrc) {
    // A memory DC has no surface until a bitmap is selected into it, which matches GDI: a
    // fresh memory DC holds a 1x1 monochrome default. Inheriting the source DC's map mode
    // is what GDI does NOT do (a new DC is always MM_TEXT), and MM_TEXT is what the engine
    // wants here anyway - DrawBody addresses the buffer in client pixels.
    m_nDcMapMode = MM_TEXT;
    m_cgCtx = 0;
    m_isMemDC = TRUE;
    // The stretch mode and brush origin come along for the ride in GDI; neither changes
    // anything that is modelled.
    (void)pSrc;
    return TRUE;
}

CBitmap* CDC::SelectObject(CBitmap* p) {
    CBitmap* prev = m_pBitmap;
    // SelectObject(NULL) is how the engine unselects at the end of DrawBody. It must not
    // clear the surface, because MFC's own idiom is SelectObject(bmpOld) where bmpOld may
    // legitimately be NULL - the DC is being discarded either way.
    if (!p) return prev;

    HBITMAP hb = (HBITMAP)*p;
    Surface* s = SurfaceFor(hb);
    if (!s) {
        // A CBitmap that was never backed by a surface. Reported rather than ignored: it
        // means everything drawn into this DC afterwards is discarded, and that is exactly
        // the failure this file exists to remove.
        fprintf(stderr, "cgsurface: SelectObject on a bitmap with no surface (%p)\n",
                (void*)hb);
        return prev;
    }
    m_pBitmap = p;
    m_cgCtx = (void*)s->ctx;
    // Surfaces are created with the flip already installed (NewSurface), because the engine
    // addresses them in client coordinates.
    m_yDown = TRUE;
    return prev;
}

// --- blitting --------------------------------------------------------------

namespace {

// The source DC's surface as an image. Returns NULL when the DC has no surface, which is
// the measurement-only case the harness creates.
CGImageRef ImageOfDC(CDC* dc) {
    if (!dc || !dc->m_cgCtx) return 0;
    return CGBitmapContextCreateImage((CGContextRef)dc->m_cgCtx);
}

// Writes the offscreen buffer to a PNG when COMIC_CHAT_DUMP_SURFACES names a directory.
//
// Kept because it is what actually located the image-orientation bug. A pane that comes out
// wrong could be the engine's geometry, the compositing, or the final blit, and those look
// identical from the outside; seeing the buffer BEFORE the blit splits the question in half.
// In that case the buffer held correctly-placed but individually mirrored sprites, which
// ruled out the engine immediately.
void MaybeDumpSurface(CGImageRef img) {
    const char* dir = getenv("COMIC_CHAT_DUMP_SURFACES");
    if (!dir || !*dir || !img) return;
    static int n = 0;
    char path[1024];
    snprintf(path, sizeof(path), "%s/surface%02d.png", dir, n++);

    CFStringRef sp = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, sp, kCFURLPOSIXPathStyle, false);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, NULL);
    if (dst) {
        CGImageDestinationAddImage(dst, img, NULL);
        CGImageDestinationFinalize(dst);
        CFRelease(dst);
    }
    CFRelease(url);
    CFRelease(sp);
    fprintf(stderr, "cgsurface: dumped %s\n", path);
}

} // namespace

BOOL CDC::BitBlt(int x, int y, int w, int h, CDC* pSrc, int xSrc, int ySrc, DWORD rop) {
    if (!m_cgCtx) return TRUE;            // measurement-only DC: nothing to present to
    CGImageRef img = ImageOfDC(pSrc);
    if (!img) return FALSE;
    MaybeDumpSurface(img);

    // A source offset means blitting a sub-rectangle. Cropping in IMAGE space needs the
    // source's own top-down row order, which is the opposite of the CGImage's, so the crop
    // rect's y is measured from the far edge.
    CGImageRef use = img;
    if (xSrc != 0 || ySrc != 0) {
        size_t ih = CGImageGetHeight(img);
        CGRect crop = CGRectMake(xSrc, (CGFloat)ih - ySrc - h, w, h);
        CGImageRef sub = CGImageCreateWithImageInRect(img, crop);
        if (sub) { CFRelease(img); use = sub; }
    }

    // Every rop the engine uses here is SRCCOPY. Others are reported rather than silently
    // treated as a copy: a wrong raster op is a visual difference that would be blamed on
    // the drawing code instead of on this line.
    if (rop != SRCCOPY)
        fprintf(stderr, "cgsurface: BitBlt with rop 0x%08lX treated as SRCCOPY\n",
                (unsigned long)rop);

    NativeDrawImage((CGContextRef)m_cgCtx, use, x, y, w, h, m_yDown != FALSE);
    CFRelease(use);
    return TRUE;
}

BOOL CDC::StretchBlt(int x, int y, int w, int h, CDC* pSrc,
                     int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop) {
    if (!m_cgCtx) return TRUE;
    CGImageRef img = ImageOfDC(pSrc);
    if (!img) return FALSE;

    CGImageRef use = img;
    if (xSrc != 0 || ySrc != 0 || wSrc != (int)CGImageGetWidth(img) ||
        hSrc != (int)CGImageGetHeight(img)) {
        size_t ih = CGImageGetHeight(img);
        CGRect crop = CGRectMake(xSrc, (CGFloat)ih - ySrc - hSrc, wSrc, hSrc);
        CGImageRef sub = CGImageCreateWithImageInRect(img, crop);
        if (sub) { CFRelease(img); use = sub; }
    }
    if (rop != SRCCOPY)
        fprintf(stderr, "cgsurface: StretchBlt with rop 0x%08lX treated as SRCCOPY\n",
                (unsigned long)rop);

    NativeDrawImage((CGContextRef)m_cgCtx, use, x, y, w, h, m_yDown != FALSE);
    CFRelease(use);
    return TRUE;
}
