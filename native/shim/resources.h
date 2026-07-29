// resources.h - Win32's binary resource API, over chat.rc compiled in.
//
// The engine loads real content through FindResource/LoadResource/LockResource: the emotion
// wheel's eight face icons (CDIB::Load(WORD), dib.cpp:164), the toolbar and tab-bar strips,
// the member-list icons. Before this existed those calls returned NULL and
// CBodyCamIcons::GetIcon then drew a CDIB with no bits - a segfault, not a blank icon.
//
// Nothing had to be invented to fix that, and nothing is read at runtime. rc.exe compiled
// chat.rc into the original .exe's resource section; native/gen-rcdata.py compiles the same
// declarations into native/shim/rcdata.cpp as static arrays, and the four functions below
// serve them. An RC custom-type resource embeds its file VERBATIM, BITMAPFILEHEADER included,
// which is exactly what dib.cpp reads off the pointer - so these are the bytes Windows
// returned rather than a re-encoding of them.
//
// See rcdata.h. The string table is the same data, reached through CString::LoadString; see
// stringtable.h.

#ifndef NATIVE_SHIM_RESOURCES_H
#define NATIVE_SHIM_RESOURCES_H

#include "win32types.h"

// How many binary resources are compiled in. For diagnostics.
//
// NOT ResourceCount: that name is a typedef in Carbon's CarbonCore/Resources.h, which arrives
// through ApplicationServices in any translation unit that also draws. The collision only
// appears in those units, so it is worth avoiding by name rather than by include order.
int ResourceManifestCount();

#endif // NATIVE_SHIM_RESOURCES_H
