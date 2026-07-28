#!/usr/bin/env python3
"""Pair the linker's demangled undefined-symbol list with nm's mangled spellings.

Reads  /tmp/undef-demangled.txt  (authoritative: what is actually missing)
       /tmp/undef-mangled.txt    (over-inclusive: also names libc/libc++abi symbols)
Writes /tmp/targets.json         [[mangled, demangled], ...] for gen-uistubs.py

The mangled name is kept EXACTLY as nm prints it, leading underscore included: clang uses
an explicit __asm__("...") label verbatim rather than prepending the platform underscore,
so a stripped underscore silently defines the wrong symbol and the link still fails.

A C symbol (no _Z prefix) does not demangle, so its "demangled" form is the name itself
minus the platform underscore - which is how the linker prints it too.
"""
import json, os, subprocess, sys

def read(path):
    with open(path) as f:
        return [l.rstrip('\n') for l in f if l.strip()]

wanted = read('/tmp/undef-demangled.txt')
mangled = read('/tmp/undef-mangled.txt')

# Demangle nm's list in one pass. Strip ONE leading underscore for the demangler (nm
# prints the platform underscore on top of the _Z prefix) but remember the full spelling.
stripped = [m[1:] if m.startswith('_') else m for m in mangled]
tool = 'c++filt'
try:
    proc = subprocess.run([tool, '-n'], input='\n'.join(stripped),
                          capture_output=True, text=True, check=True)
except (FileNotFoundError, subprocess.CalledProcessError):
    tool = 'llvm-cxxfilt'
    proc = subprocess.run([tool], input='\n'.join(stripped),
                          capture_output=True, text=True, check=True)
demangled = proc.stdout.split('\n')

# demangled[i] corresponds to mangled[i]. Build the reverse map, preferring the first
# spelling seen so the output is stable.
spelling = {}
for full, dem in zip(mangled, demangled):
    spelling.setdefault(dem.strip(), full)

targets, missing = [], []
for w in wanted:
    # A C++ symbol matches its demangled form directly. A C symbol does not demangle, so
    # the demangler pass hands back the name with the platform underscore already
    # stripped, while the linker prints it WITH the underscore ("_ConferenceConnectA").
    # Try both spellings rather than assuming which kind this is.
    key = w if w in spelling else w.lstrip('_')
    if key in spelling:
        targets.append((spelling[key], w))
    else:
        missing.append(w)

with open('/tmp/targets.json', 'w') as f:
    json.dump(targets, f, indent=1)

print(f"matched {len(targets)} of {len(wanted)} via {tool}")
if missing:
    # Not fatal, but it means uistubs.cpp will not define these and the link will still
    # fail naming them - better to say so here than to look like a clean regeneration.
    print(f"UNMATCHED ({len(missing)}) - these will remain undefined:")
    for m in missing[:20]:
        print(f"    {m}")
    sys.exit(1)
