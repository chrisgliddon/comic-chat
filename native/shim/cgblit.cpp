// cgblit.cpp - ::StretchDIBits over Core Graphics.
//
// This is what lets the ENGINE composite its own avatar bodies instead of a reimplementation
// guessing at the result. CDIB::Draw (dib.cpp:208-258) funnels all three overloads through
// ::StretchDIBits with a raster op, and bodycam.cpp draws a body as a sequence of those
// calls whose order depends on the avatar's TORSOFIRST/TORSOMASK/HEADMASK flags. Getting
// that ordering right is the engine's job; getting pixels onto a CGContext is this file's.
//
// THE HARD PART: Core Graphics has no raster ops. GDI has no alpha. The engine bridges that
// gap the way every Win16-era sprite blitter did, as a PAIR of calls:
//
//     mask   ->  MERGEPAINT   dst = ~src | dst
//     drawing->  SRCAND       dst =  src & dst
//
// The mask is black over the sprite's silhouette and white elsewhere, and the drawing is
// the sprite on a white background. Trace it through: MERGEPAINT turns the silhouette WHITE
// in the destination and leaves everything else alone; SRCAND then writes the sprite where
// the destination is white and preserves the destination where the drawing is white. Net
// result: the sprite appears inside its silhouette and the background survives around it.
//
// So the honest translation is not to emulate two raster ops - it is to recognise the pair
// and do in one alpha composite what GDI needed two passes for. The mask arrives first and
// is held on the DC (m_pendMask*) until the SRCAND that consumes it.
//
// A drawing arriving with no pending mask still draws: palette index 0 is Comic Chat's
// transparent key, so it is used as alpha. That covers poses that ship without a mask.

#include "stdafx.h"
#include "render.h"          // brings in ApplicationServices, and #undefs MAX/MIN

#include <vector>

namespace {

struct Bmp {
    int w, h, bpp, stride;
    const BYTE* bits;
    const RGBQUAD* pal;
    bool topDown;
};

bool Describe(const void* bits, const BITMAPINFO* bmi, Bmp& o) {
    if (!bits || !bmi) return false;
    const BITMAPINFOHEADER& h = bmi->bmiHeader;
    o.w = (int)h.biWidth;
    o.h = (int)(h.biHeight < 0 ? -h.biHeight : h.biHeight);
    o.topDown = h.biHeight < 0;          // a negative height means top-down rows
    o.bpp = (int)h.biBitCount;
    if (o.w <= 0 || o.h <= 0) return false;
    // DIB rows are DWORD-aligned. Getting this wrong shears the image progressively, which
    // is a very recognisable symptom and worth naming here.
    o.stride = ((o.w * o.bpp + 31) / 32) * 4;
    o.bits = (const BYTE*)bits;
    o.pal = bmi->bmiColors;
    return true;
}

// Reads one pixel as 8-bit grey plus its palette index. Only the depths the engine's poses
// and masks actually use are handled; anything else reports "not covered" so a surprise
// format is visible rather than silently blank.
bool SamplePixel(const Bmp& b, int x, int y, unsigned char* r, unsigned char* g,
                 unsigned char* bl, int* index) {
    if (x < 0 || y < 0 || x >= b.w || y >= b.h) return false;
    int row = b.topDown ? y : (b.h - 1 - y);
    const BYTE* p = b.bits + (size_t)row * b.stride;
    switch (b.bpp) {
        case 1: {
            int bit = (p[x >> 3] >> (7 - (x & 7))) & 1;
            *index = bit;
            const RGBQUAD& c = b.pal[bit];
            *r = c.rgbRed; *g = c.rgbGreen; *bl = c.rgbBlue;
            return true;
        }
        case 4: {
            int idx = (x & 1) ? (p[x >> 1] & 0x0F) : (p[x >> 1] >> 4);
            *index = idx;
            const RGBQUAD& c = b.pal[idx];
            *r = c.rgbRed; *g = c.rgbGreen; *bl = c.rgbBlue;
            return true;
        }
        case 8: {
            int idx = p[x];
            *index = idx;
            const RGBQUAD& c = b.pal[idx];
            *r = c.rgbRed; *g = c.rgbGreen; *bl = c.rgbBlue;
            return true;
        }
        case 24: {
            const BYTE* q = p + (size_t)x * 3;
            *bl = q[0]; *g = q[1]; *r = q[2];   // DIBs are BGR
            *index = -1;
            return true;
        }
        case 32: {
            const BYTE* q = p + (size_t)x * 4;
            *bl = q[0]; *g = q[1]; *r = q[2];
            *index = -1;
            return true;
        }
        default:
            return false;
    }
}

// Draws `src` into ctx at the given engine-space rect, with alpha taken from `mask` when
// one is supplied (silhouette = mask BLACK) and from palette index 0 otherwise.
// `opaque` forces every pixel solid, which is what SRCCOPY means. Without it a backdrop drawn
// with SRCCOPY would lose every pixel whose palette index happens to be 0 - and for a
// full-frame background that is usually a real colour, not a transparency key.
void Composite(CGContextRef ctx, const Bmp& src, const Bmp* mask,
               int x, int y, int w, int h, bool opaque, bool yDown) {
    std::vector<unsigned char> rgba((size_t)src.w * src.h * 4, 0);

    for (int py = 0; py < src.h; py++) {
        unsigned char* dst = &rgba[(size_t)py * src.w * 4];
        for (int px = 0; px < src.w; px++) {
            unsigned char r = 0, g = 0, b = 0;
            int idx = -1;
            if (!SamplePixel(src, px, py, &r, &g, &b, &idx)) continue;

            bool solid;
            if (opaque) {
                solid = true;
            } else if (mask) {
                // Mask sampled at the same normalised position: a mask can be a different
                // pixel size from its drawing, and the engine stretches both to one rect.
                int mx = mask->w == src.w ? px : (int)((long)px * mask->w / src.w);
                int my = mask->h == src.h ? py : (int)((long)py * mask->h / src.h);
                unsigned char mr = 255, mg = 255, mb = 255;
                int mi = -1;
                if (!SamplePixel(*mask, mx, my, &mr, &mg, &mb, &mi)) solid = false;
                else solid = (mr + mg + mb) < 384;       // black-ish = inside the silhouette
            } else {
                solid = (idx != 0);                      // index 0 is the transparent key
            }
            if (!solid) continue;
            dst[px * 4 + 0] = r;
            dst[px * 4 + 1] = g;
            dst[px * 4 + 2] = b;
            dst[px * 4 + 3] = 255;
        }
    }

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef bmp = CGBitmapContextCreate(&rgba[0], src.w, src.h, 8,
                                             (size_t)src.w * 4, cs,
                                             kCGImageAlphaPremultipliedLast);
    CGImageRef img = bmp ? CGBitmapContextCreateImage(bmp) : NULL;
    if (img) {
        NativeDrawImage(ctx, img, x, y, w, h, yDown);
        CFRelease(img);
    }
    if (bmp) CFRelease(bmp);
    CFRelease(cs);
}

// Paints WHITE wherever the source is black, leaving the rest untouched - MERGEPAINT's actual
// effect. Used for the halo and the mask pass; see the call site.
void PaintWhereBlack(CGContextRef ctx, const Bmp& src, int x, int y, int w, int h,
                     bool yDown) {
    std::vector<unsigned char> rgba((size_t)src.w * src.h * 4, 0);
    for (int py = 0; py < src.h; py++) {
        unsigned char* dst = &rgba[(size_t)py * src.w * 4];
        for (int px = 0; px < src.w; px++) {
            unsigned char r = 0, g = 0, b = 0;
            int idx = -1;
            if (!SamplePixel(src, px, py, &r, &g, &b, &idx)) continue;
            if ((r + g + b) >= 384) continue;          // not black-ish: leave the destination
            dst[px * 4 + 0] = 255;
            dst[px * 4 + 1] = 255;
            dst[px * 4 + 2] = 255;
            dst[px * 4 + 3] = 255;
        }
    }
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef bmp = CGBitmapContextCreate(&rgba[0], src.w, src.h, 8, (size_t)src.w * 4, cs,
                                             kCGImageAlphaPremultipliedLast);
    CGImageRef img = bmp ? CGBitmapContextCreateImage(bmp) : NULL;
    if (img) {
        NativeDrawImage(ctx, img, x, y, w, h, yDown);
        CFRelease(img);
    }
    if (bmp) CFRelease(bmp);
    CFRelease(cs);
}

} // namespace

int StretchDIBits(HDC hdc, int xDst, int yDst, int wDst, int hDst,
                  int /*xSrc*/, int /*ySrc*/, int /*wSrc*/, int /*hSrc*/,
                  const void* bits, const BITMAPINFO* bmi, UINT /*usage*/, DWORD rop) {
    // GetSafeHdc returns the CDC itself (see the CDC constructor), so the target context
    // and the pending-mask state are reachable from here.
    CDC* dc = (CDC*)hdc;
    if (!dc) return 0;
    CGContextRef ctx = (CGContextRef)dc->m_cgCtx;

    // A measurement-only DC paints nothing. This is the property that keeps every golden
    // out of reach of anything in this file.
    if (!ctx) return 0;

    Bmp src;
    if (!Describe(bits, bmi, src)) return 0;

    if (rop == MERGEPAINT) {
        // MERGEPAINT is `dst = ~src | dst`, so where the source is BLACK the destination goes
        // WHITE and elsewhere it is untouched. Doing that literally is both halves of the job:
        //
        //   the HALO. bodycam.cpp:534-541 blits each pose's aura with MERGEPAINT before the
        //   body, and the aura is black over a dilated silhouette. Whitening there is the
        //   "halo" of SIGGRAPH 96 section 4.4 - the margin that makes a character readable
        //   against a busy background. Skipping it left the avatars tangled in the backdrop
        //   hatching, which is exactly what the paper says halos exist to prevent.
        //
        //   the MASK. The same op is used for a pose's mask immediately before its drawing.
        //   Whitening the silhouette there is harmless, because the SRCAND that follows
        //   overwrites those pixels with the drawing itself.
        //
        // So paint it, AND remember it as the pending mask for the SRCAND that may follow.
        PaintWhereBlack(ctx, src, xDst, yDst, wDst, hDst, dc->m_nDcMapMode == MM_TEXT);

        dc->m_pendMaskBits = bits;
        dc->m_pendMaskInfo = bmi;
        dc->m_pendMaskX = xDst; dc->m_pendMaskY = yDst;
        dc->m_pendMaskW = wDst; dc->m_pendMaskH = hDst;
        return src.h;
    }

    const Bmp* maskPtr = NULL;
    Bmp mask;
    if (rop == SRCAND && dc->m_pendMaskBits &&
        dc->m_pendMaskX == xDst && dc->m_pendMaskY == yDst &&
        dc->m_pendMaskW == wDst && dc->m_pendMaskH == hDst) {
        if (Describe(dc->m_pendMaskBits, dc->m_pendMaskInfo, mask)) maskPtr = &mask;
    }
    // Consumed either way: a mask that did not match its drawing's rect must not leak into
    // the next sprite.
    dc->m_pendMaskBits = 0;
    dc->m_pendMaskInfo = 0;

    // SRCCOPY replaces the destination, so nothing in the source is transparent. The mask pair
    // (MERGEPAINT then SRCAND) is the only path that carries transparency.
    Composite(ctx, src, maskPtr, xDst, yDst, wDst, hDst, rop == SRCCOPY,
              dc->m_nDcMapMode == MM_TEXT);
    return src.h;
}
