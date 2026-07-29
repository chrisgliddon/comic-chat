#!/usr/bin/env python3
"""Compile chat.rc's resources into a C source file - what rc.exe did.

The original has no data files for any of this. rc.exe reads chat.rc, and the linker puts
the result in the .exe's resource section; CDIB::Load(WORD) and CString::LoadString then
reach it through FindResource and LoadString. A Mach-O binary has no resource section, but
that is a container problem, not a reason to invent a format: the fix is to put the same
bytes in the binary a different way, and leave the engine's own resource calls alone.

So this emits native/shim/rcdata.cpp - static arrays, compiled in and linked. FindResource
returns a pointer into them, exactly as LockResource returns a pointer into a PE resource,
and dib.cpp's comment that it is "not required to unlock or free the resource in Win32"
stays true. Nothing is read from disk and nothing can go missing from a bundle.

Covers the two resource types the engine reads CONTENT from:

  STRINGTABLE           Not just labels. textpose.cpp's InitializeEmotionRules loads the
                        emotion-detection RULES from it (ID_RULE_SHOUT and friends), so
                        without these the native build has no emotion rules at all and every
                        pose decision differs from the goldens.

  BITMAP / DIB / ICON   The emotion wheel's eight face icons, the toolbar and tab-bar strips,
                        the member-list icons. An RC custom-type resource embeds the file
                        VERBATIM, BITMAPFILEHEADER included, which is exactly what dib.cpp
                        reads off the pointer - so the bytes here are the bytes Windows
                        returned, not a re-encoding.

NOT covered: the frozen glyph table (oracle/glyphs/glyphs.json). That one is not a resource
and never existed in the original - it is the measurement oracle RULEBOOK 5 requires, captured
from Windows GDI, and it stays a data file because it is verification data rather than
program content.

There is a natural check on the string extraction: with these strings compiled in, the
native --textpose dump must still reproduce oracle/textpose/textpose.golden.json, which was
captured on Windows from the real resource table. A mangled escape changes the rules and that
diff fails.

RC string syntax handled:
  * outer double quotes delimit; "" inside means a literal quote
  * C escapes \\n \\t \\r \\\\ \\" and \\ooo octal
  * adjacent string literals on one line concatenate
  * a trailing backslash or an unterminated line continues onto the next
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TREE = os.path.join(ROOT, 'v2.5-beta-1-modern')
OUT  = os.path.join(ROOT, 'native', 'shim', 'rcdata.cpp')


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


def unescape_rc(body):
    """Turn an RC string literal body into its runtime bytes."""
    out = []
    i = 0
    while i < len(body):
        c = body[i]
        if c == '"' and i + 1 < len(body) and body[i + 1] == '"':
            out.append('"')
            i += 2
            continue
        if c != '\\':
            out.append(c)
            i += 1
            continue
        i += 1
        if i >= len(body):
            break
        e = body[i]
        # Octal first: RC uses \ooo, and \0 is a legitimate embedded NUL.
        if e in '01234567':
            j = i
            while j < len(body) and j < i + 3 and body[j] in '01234567':
                j += 1
            out.append(chr(int(body[i:j], 8) & 0xFF))
            i = j
            continue
        simple = {'n': '\n', 't': '\t', 'r': '\r', '\\': '\\', '"': '"', 'a': '\a'}
        if e == 'x':
            j = i + 1
            while j < len(body) and body[j] in '0123456789abcdefABCDEF':
                j += 1
            out.append(chr(int(body[i + 1:j], 16) & 0xFF))
            i = j
            continue
        out.append(simple.get(e, e))
        i += 1
    return ''.join(out)


def parse_stringtable(rc_path, ids):
    """(id, bytes) for every STRINGTABLE entry."""
    lines = open(rc_path, encoding='latin-1').read().split('\n')
    strings = []
    in_table = False
    depth = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if re.match(r'^STRINGTABLE\b', stripped):
            in_table = True
            i += 1
            continue
        if in_table:
            if stripped == 'BEGIN' or stripped == '{':
                depth += 1
                i += 1
                continue
            if stripped == 'END' or stripped == '}':
                depth -= 1
                if depth <= 0:
                    in_table = False
                i += 1
                continue
            if depth > 0 and stripped and not stripped.startswith('//'):
                # NAME  "text"    - the text may continue over following lines.
                m = re.match(r'^([A-Za-z_]\w*|\d+)\s+(.*)$', stripped)
                if m:
                    key, rest = m.group(1), m.group(2)
                    # Gather quoted pieces, continuing while the line is unterminated.
                    buf = rest
                    while buf.count('"') % 2 == 1 and i + 1 < len(lines):
                        i += 1
                        buf += '\n' + lines[i].strip()
                    pieces = re.findall(r'"((?:[^"]|"")*)"', buf)
                    if pieces:
                        text = ''.join(unescape_rc(p) for p in pieces)
                        rid = ids.get(key)
                        if rid is None and re.fullmatch(r'\d+', key):
                            rid = int(key)
                        if rid is not None:
                            strings.append((rid, text))
            i += 1
            continue
        i += 1
    return strings


# NAME TYPE [memory flags] "file"
# The memory flags (DISCARDABLE, MOVEABLE, PURE, PRELOAD) are 16-bit-era hints with no
# bearing on the content, so they are matched and dropped.
BINDECL = re.compile(
    r'^([A-Za-z_]\w*)\s+(BITMAP|DIB|ICON|CURSOR|AVI)\s+'
    r'((?:DISCARDABLE|MOVEABLE|PURE|PRELOAD|FIXED|LOADONCALL)\s+)*'
    r'"([^"]+)"\s*$')


def parse_binaries(rc_path, ids):
    """(id, type, symbol, filepath) for every BITMAP/DIB/ICON declaration."""
    out, missing, unresolved = [], [], []
    for line in open(rc_path, encoding='latin-1'):
        m = BINDECL.match(line.strip())
        if not m:
            continue
        name, rtype, _flags, path = m.group(1), m.group(2), m.group(3), m.group(4)
        if name not in ids:
            unresolved.append(name)
            continue
        rel = path.replace('\\\\', '/').replace('\\', '/')
        full = os.path.join(TREE, rel)
        if not os.path.exists(full):
            missing.append(rel)
            continue
        out.append((ids[name], rtype, name, full))
    return out, missing, unresolved


def c_bytes(data, indent='    '):
    """A byte array body, 16 per line."""
    lines = []
    for off in range(0, len(data), 16):
        chunk = data[off:off + 16]
        lines.append(indent + ','.join('0x%02X' % b for b in chunk) + ',')
    return '\n'.join(lines)


def c_string_literal(text):
    """A C string literal for arbitrary bytes, including embedded NULs."""
    out = []
    for ch in text.encode('latin-1', errors='replace'):
        if ch == 0x22:
            out.append('\\"')
        elif ch == 0x5C:
            out.append('\\\\')
        elif ch == 0x0A:
            out.append('\\n')
        elif ch == 0x0D:
            out.append('\\r')
        elif ch == 0x09:
            out.append('\\t')
        elif 0x20 <= ch < 0x7F:
            out.append(chr(ch))
        else:
            # Octal with all three digits, so a following digit cannot extend the escape.
            out.append('\\%03o' % ch)
    return '"' + ''.join(out) + '"'


def main():
    rc = os.path.join(TREE, 'chat.rc')
    ids = parse_resource_h(os.path.join(TREE, 'resource.h'))

    strings = parse_stringtable(rc, ids)
    binaries, missing, unresolved = parse_binaries(rc, ids)

    total = 0
    with open(OUT, 'w') as f:
        f.write('// rcdata.cpp - GENERATED by native/gen-rcdata.py. Do not edit.\n')
        f.write('//\n')
        f.write('// chat.rc\'s resources, compiled in - which is what rc.exe and the linker did\n')
        f.write('// for the original. See gen-rcdata.py for why this is a C file and not a data\n')
        f.write('// file, and native/shim/rcdata.h for how it is read.\n')
        f.write('//\n')
        f.write('// Regenerate after editing chat.rc, resource.h or anything under res/.\n\n')
        f.write('#include "rcdata.h"\n\n')

        f.write('// --- STRINGTABLE ----------------------------------------------------------\n')
        f.write('const NativeRcString kNativeRcStrings[] = {\n')
        for rid, text in sorted(strings):
            f.write('    { %d, %s },\n' % (rid, c_string_literal(text)))
        f.write('};\n')
        f.write('const int kNativeRcStringCount =\n')
        f.write('    (int)(sizeof(kNativeRcStrings) / sizeof(kNativeRcStrings[0]));\n\n')

        f.write('// --- BITMAP / DIB / ICON --------------------------------------------------\n')
        f.write('// Each file VERBATIM, BITMAPFILEHEADER and all, because that is what an RC\n')
        f.write('// custom-type resource contained and what dib.cpp reads off the pointer.\n\n')
        for rid, rtype, sym, path in binaries:
            data = open(path, 'rb').read()
            total += len(data)
            f.write('// %s (%s) - %s, %d bytes\n'
                    % (sym, rtype, os.path.relpath(path, TREE), len(data)))
            f.write('static const unsigned char kRc_%s_%s[] = {\n' % (sym, rtype))
            f.write(c_bytes(data))
            f.write('\n};\n\n')

        f.write('const NativeRcBinary kNativeRcBinaries[] = {\n')
        for rid, rtype, sym, path in binaries:
            f.write('    { %d, "%s", "%s", kRc_%s_%s, (unsigned long)sizeof(kRc_%s_%s) },\n'
                    % (rid, rtype, sym, sym, rtype, sym, rtype))
        f.write('};\n')
        f.write('const int kNativeRcBinaryCount =\n')
        f.write('    (int)(sizeof(kNativeRcBinaries) / sizeof(kNativeRcBinaries[0]));\n')

    print('wrote %s' % os.path.relpath(OUT, ROOT))
    print('  %d strings, %d binaries (%d bytes embedded)' % (len(strings), len(binaries), total))
    if unresolved:
        print('  ids not found in resource.h: %s' % ', '.join(sorted(set(unresolved))))
    # A declared file that is absent would leave a pane blank for a reason that is not
    # visible from the app, so it is reported rather than skipped quietly.
    if missing:
        print('  declared but missing on disk: %s' % ', '.join(sorted(set(missing))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
