#!/bin/sh
# gen-targets.sh - build /tmp/targets.json, then regenerate native/uistubs.cpp.
#
# This step used to be ad-hoc, which was a mistake: it has to be repeated every time a
# new engine file starts compiling (each one resolves some symbols and introduces
# references to others), and getting it wrong produces a stub file that either misses
# symbols or defines ones the real code now provides.
#
# gen-uistubs.py needs (mangled, demangled) PAIRS. Neither tool alone gives both:
#
#   * The linker prints DEMANGLED names, and its list is authoritative - it is exactly
#     the set that is missing after everything else has been resolved.
#   * `nm -u` prints MANGLED names, which is what an asm label needs, but its list is
#     over-inclusive: it also names everything resolved later by libc and libc++abi.
#
# So take the linker's set as the truth and use nm only to recover the spelling. Run
# under sh, not zsh: zsh does not word-split unquoted expansions, so native_objs() there
# collapses to one bogus filename and the link fails for the wrong reason - which once
# produced a triumphant "0 undefined symbols" from a link with no inputs.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
. ./native/units.sh

OBJS="$(native_objs) native/build/oracleharness.o"

# 1. The authoritative list, demangled.
clang++ -o /dev/null $OBJS native/shim/msvcrand.cpp -lz 2>&1 |
  grep '^  "' | sed 's/^  "//;s/", referenced.*//' | sort -u > /tmp/undef-demangled.txt

# 2. Mangled undefined symbols from the same objects, to recover the spelling.
nm -u $OBJS 2>/dev/null | sed 's/^ *//' | grep '^_' | sort -u > /tmp/undef-mangled.txt

echo "linker wants $(wc -l < /tmp/undef-demangled.txt | tr -d ' ') symbols;" \
     "nm offers $(wc -l < /tmp/undef-mangled.txt | tr -d ' ') candidate spellings"

python3 ./native/match-targets.py
python3 ./native/gen-uistubs.py
