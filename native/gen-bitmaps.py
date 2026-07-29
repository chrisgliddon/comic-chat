#!/usr/bin/env python3
"""Extract chat.rc's binary resources (BITMAP, DIB, ICON) into a manifest.

Companion to gen-strings.py, and for the same reason: a Mach-O binary has no PE resource
section, but the engine reads real content out of one. CDIB::Load(WORD) (dib.cpp:164) goes
FindResource -> LoadResource -> LockResource and then reads a BITMAPFILEHEADER off the
result, so an unimplemented FindResource means the emotion-wheel face icons never load and
CBodyCam::DrawBullsEye dereferences a CDIB with no bits.

The files themselves are all present under v2.5-beta-1-modern/res, so nothing needs to be
invented - only the id-to-filename mapping, which chat.rc states outright:

    IDR_HAPPY               DIB     DISCARDABLE     "res\\\\fc_hap_l.bmp"

An RC custom-type resource embeds the file VERBATIM, BITMAPFILEHEADER included, which is
exactly what dib.cpp expects at the resource pointer. So serving the file's bytes unchanged
is not an approximation of what Windows did - it is the same bytes.

Output: native/resources/bitmaps.json
    { "<id>": { "type": "DIB", "file": "res/fc_hap_l.bmp" }, ... }

Ids are numbers, resolved through resource.h. Paths are forward-slashed and relative to the
resource root the shim is given (the v2.5 tree, or an .app's Contents/Resources).
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TREE = os.path.join(ROOT, 'v2.5-beta-1-modern')

# Same #define walker as gen-strings.py. Duplicated rather than shared because the file
# names carry a dash and cannot be imported as modules; it is 15 lines.
def parse_resource_h(path):
    """name -> numeric id, following simple #define chains."""
    ids = {}
    pat = re.compile(r'^\s*#define\s+([A-Za-z_]\w*)\s+(.+?)\s*$')
    for line in open(path, encoding='latin-1'):
        m = pat.match(line)
        if not m:
            continue
        name, val = m.group(1), m.group(2).split('//')[0].split('/*')[0].strip()
        if re.fullmatch(r'0[xX][0-9a-fA-F]+', val):
            ids[name] = int(val, 16)
        elif re.fullmatch(r'-?\d+', val):
            ids[name] = int(val)
        elif val in ids:
            ids[name] = ids[val]
    return ids

# NAME TYPE [memory flags] "file"
# The memory flags (DISCARDABLE, MOVEABLE, PURE, PRELOAD) are 16-bit-era hints with no
# bearing on the content, so they are matched and dropped.
DECL = re.compile(
    r'^([A-Za-z_]\w*)\s+(BITMAP|DIB|ICON|CURSOR|AVI)\s+'
    r'((?:DISCARDABLE|MOVEABLE|PURE|PRELOAD|FIXED|LOADONCALL)\s+)*'
    r'"([^"]+)"\s*$')

def main():
    rc = os.path.join(TREE, 'chat.rc')
    ids = parse_resource_h(os.path.join(TREE, 'resource.h'))

    out, missing, unresolved = {}, [], []
    for line in open(rc, encoding='latin-1'):
        m = DECL.match(line.strip())
        if not m:
            continue
        name, rtype, _flags, path = m.group(1), m.group(2), m.group(3), m.group(4)
        if name not in ids:
            unresolved.append(name)
            continue
        rel = path.replace('\\\\', '/').replace('\\', '/')
        if not os.path.exists(os.path.join(TREE, rel)):
            missing.append(rel)
            continue
        # One id can carry two resources of DIFFERENT types - IDR_MAINFRAME is both the
        # app ICON and the toolbar BITMAP. The key is therefore id+type, not id alone.
        out['%d:%s' % (ids[name], rtype)] = {'type': rtype, 'file': rel, 'name': name}

    dest = os.path.join(ROOT, 'native', 'resources', 'bitmaps.json')
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, 'w') as f:
        json.dump(out, f, indent=1, sort_keys=True)

    print('wrote %s: %d resources' % (os.path.relpath(dest, ROOT), len(out)))
    if unresolved:
        print('  ids not found in resource.h: %s' % ', '.join(sorted(set(unresolved))))
    # A declared file that is absent is worth reporting rather than skipping silently: it
    # means a pane will come up blank and the reason will not be obvious from the app.
    if missing:
        print('  declared but missing on disk: %s' % ', '.join(sorted(set(missing))))
    return 0

if __name__ == '__main__':
    sys.exit(main())
