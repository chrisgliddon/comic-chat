// avbmain.cpp - native macOS driver for the Tier-2 asset manifest dump.
//
// This is milestone 1 of the native port and the first real proof that the shim
// layer is correct rather than merely compiling. It runs the SAME dump code as the
// Windows oracle harness (oracle/harness/avbdump.cpp) over the SAME ComicArt
// assets, so its output can be diffed byte-for-byte against the 33 frozen
// oracle/avb/*.golden.json.
//
// What a clean diff proves, all at once:
//
//   * Integer widths and struct packing. avbfile.h typedefs AVBINT32 from ULONG
//     inside #pragma pack(push, 1) and memcpys AVATARFACEDATA/BODYDATA/TORSODATA
//     straight out of the file. If the shim's ULONG were `unsigned long` (8 bytes
//     on arm64 rather than 4), every offset in every asset would be read from the
//     wrong bytes and every manifest would diverge.
//   * The zlib framing and the 2-bpp maskedmono/dualmask expansion, via 1612
//     decoded image slots and ~13.8 MB of hashed pixels.
//   * That CDIB's accessors, DIBStorageWidth's sub-byte rounding, and the
//     row-masked pixel hash all behave identically off-Windows.
//
// Usage: avbdump <outDir> [artDir]
//        artDir defaults to v2.5-beta-1-modern/ComicArt relative to the repo root.

#include "stdafx.h"
#include <stdio.h>
#include <string.h>
#include <string>

#include "ojson.h"
#include "avbdump.h"

// The engine objects this links reference a global theApp. avbfile.cpp and dib.cpp
// only #include chat.h for it and never touch its state (verified: the sole
// reference in avbfile.cpp is the #include comment), so a definition is needed for
// the linker and nothing more. Deliberately NOT a CChatApp - constructing one would
// drag in the MFC application object and the whole UI tree.
//
// If a future milestone needs real app state (theApp.GetAvatarDir(), the font
// settings), that is the point at which a proper native application object should
// appear - not by growing this stub.

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <outDir> [artDir]\n", argv[0]);
        fprintf(stderr, "  writes <outDir>/<stem>.json per ComicArt asset, plus index.json\n");
        return 2;
    }
    const char* outDir = argv[1];
    const char* artDir = (argc >= 3) ? argv[2] : "v2.5-beta-1-modern/ComicArt";

    fprintf(stderr, "native avbdump: artDir=%s outDir=%s\n", artDir, outDir);

    ojson::Value index = CaptureAvb(artDir, outDir);

    char idxPath[1024];
    snprintf(idxPath, sizeof(idxPath), "%s/index.json", outDir);
    FILE* f = fopen(idxPath, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", idxPath); return 1; }
    std::string out = index.EmitToString();
    fwrite(out.c_str(), 1, out.size(), f);
    fclose(f);

    printf("avb manifests written to %s\n", outDir);
    return 0;
}
