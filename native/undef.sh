#!/bin/sh
# undef.sh - list the symbols the harness link is still missing, demangled.
#
# Feed into native/gen-uistubs.py to regenerate the stub file. Run under sh, not zsh:
# zsh does not word-split unquoted parameter expansions, so native_objs() there expands
# to one bogus filename and the link fails for the wrong reason.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
. ./native/units.sh
clang++ -o /dev/null $(native_objs) native/build/oracleharness.o \
    native/shim/msvcrand.cpp -lz 2>&1 |
  grep '^  "' | sed 's/^  "//;s/", referenced.*//' | sort -u
