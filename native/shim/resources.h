// resources.h - Win32's binary resource API, served from files.
//
// The engine loads real content through FindResource/LoadResource/LockResource: the emotion
// wheel's eight face icons (CDIB::Load(WORD), dib.cpp:164), the toolbar and tab-bar strips,
// the member-list icons. A Mach-O binary has no PE resource section, so those calls used to
// return NULL and CBodyCam::DrawBullsEye then drew a CDIB with no bits - a segfault, not a
// blank icon.
//
// Nothing has to be invented to fix that. The files are all present under the v2.5 tree's
// res/ directory, and chat.rc states which id maps to which file. native/gen-bitmaps.py
// turns those declarations into native/resources/bitmaps.json, and this serves the file's
// bytes VERBATIM - which is the same bytes Windows would have handed back, because an RC
// custom-type resource embeds the file whole, BITMAPFILEHEADER included. dib.cpp reads that
// header off the pointer it gets, and is satisfied.
//
// Companion to stringtable.h, which does the same for STRINGTABLE. Both are generated data
// that an .app must ship, because both carry behaviour rather than decoration.

#ifndef NATIVE_SHIM_RESOURCES_H
#define NATIVE_SHIM_RESOURCES_H

#include "win32types.h"

// The directory that res/ and bitmaps.json are found under - the v2.5 tree during
// development, an .app's Contents/Resources when installed. Set before any resource is
// asked for; NativeSessionStart does it.
void ResourceSetRoot(const char* dir);

// Loads the manifest if it has not been loaded. Returns false if it is missing or
// malformed, which leaves every FindResource returning NULL - the previous behaviour, so a
// missing manifest degrades rather than crashes.
bool ResourceLoadManifest();

// How many resources the manifest holds. For diagnostics.
int ResourceCount();

#endif // NATIVE_SHIM_RESOURCES_H
