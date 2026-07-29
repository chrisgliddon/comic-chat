// resources.cpp - see resources.h.

#include "stdafx.h"
#include "resources.h"
#include "rcdata.h"

#include <stdio.h>

// --- the Win32 resource API ------------------------------------------------
//
// Served straight from the linked-in arrays. HRSRC and HGLOBAL are both the NativeRcBinary*,
// because with the bytes already in the binary there is no separate "loaded" state to model -
// which is also true of a PE resource, where LoadResource on a mapped image just hands back a
// pointer into it.

HRSRC FindResource(HINSTANCE, LPCTSTR name, LPCTSTR type) {
    // MAKEINTRESOURCE packs a numeric id into the low word of a pointer. The engine only ever
    // asks by number (dib.cpp uses MAKEINTRESOURCE(wResid)), so a genuine string name is not
    // a case that arises - and treating one as an id would be a silent wrong lookup.
    UINT_PTR raw = (UINT_PTR)name;
    if (raw > 0xFFFF) {
        fprintf(stderr, "resources: FindResource by NAME (\"%s\") is not supported; chat.rc's "
                        "resources are all numbered\n", (const char*)name);
        return (HRSRC)0;
    }
    if (!type) return (HRSRC)0;

    // Matched on id AND type: one id can carry two resources of different types.
    // IDR_MAINFRAME is both the application ICON and the toolbar BITMAP.
    for (int i = 0; i < kNativeRcBinaryCount; i++) {
        const NativeRcBinary& b = kNativeRcBinaries[i];
        if (b.id != (UINT)raw) continue;
        if (strcmp(b.type, type) != 0) continue;
        return (HRSRC)&b;
    }
    return (HRSRC)0;
}

HGLOBAL LoadResource(HINSTANCE, HRSRC h) { return (HGLOBAL)h; }

void* LockResource(HGLOBAL h) {
    const NativeRcBinary* b = (const NativeRcBinary*)h;
    // The cast away from const is what the Win32 signature forces. dib.cpp reads the header
    // and copies the bits out; it does not write through this pointer, and on Windows it went
    // out of its way to VirtualProtect the pages before a path that might have.
    return b ? (void*)b->data : (void*)0;
}

DWORD SizeofResource(HINSTANCE, HRSRC h) {
    const NativeRcBinary* b = (const NativeRcBinary*)h;
    return b ? (DWORD)b->size : 0;
}

int ResourceManifestCount() { return kNativeRcBinaryCount; }
