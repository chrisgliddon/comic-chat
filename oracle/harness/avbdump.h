// avbdump.h - Tier-2 .avb/.bgb asset manifest dump.
//
// Extracted from oracleharness.cpp so that TWO binaries can run it:
//
//   * the Windows oracle harness (OracleHarness.exe --avb), which produces the
//     frozen goldens in oracle/avb/, and
//   * the native macOS build (native/avbmain.cpp).
//
// That sharing is the whole point. "The native port reproduces the frozen
// goldens" is only a strong claim if both sides run the SAME dump code against
// the same assets - otherwise a match could mean two implementations happen to
// agree, and a mismatch would not say whether the engine or the dump diverged.
// With one implementation, any difference is attributable to the platform layer.
//
// Requires: avbfile.cpp, dib.cpp, avatar.cpp, backdrop.cpp, ojson.cpp, and zlib.
// Notably does NOT require a DC, fonts, the emotion tables or the avatar registry -
// nothing here draws, which is what lets it run against the GDI stubs on macOS.

#ifndef ORACLE_AVBDUMP_H
#define ORACLE_AVBDUMP_H

#include "ojson.h"

// Writes <outDir>/<stem>.json for every .avb and .bgb in artDir, and returns the
// index value (the caller writes it as index.json). Enumeration is sorted, so the
// output does not depend on filesystem order.
ojson::Value CaptureAvb(const char* artDir, const char* outDir);

// Observability sinks called from avbfile.cpp under ORACLE_HARNESS. Declared here
// so the engine's extern declarations have a matching definition in whichever
// binary links this. See the comment block in avbfile.cpp for what each answers.
void OracleAvbTag(int tag, int size);
void OracleAvbImageRead(int slot, int format, int paletteType);
void OracleAvbPreConvert(int biCompression, unsigned long biSizeImage, int biBitCount);
void OracleAvbBackdropRecord(unsigned long offset, int format, int paletteType);

#endif // ORACLE_AVBDUMP_H
