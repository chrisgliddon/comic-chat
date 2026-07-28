#!/bin/sh
# build-harness.sh - build the full oracle harness natively (the corpus replay).
#
# Links the whole engine: balloon, panel, pageview, chatdoc, protsupp, histent, fonts -
# the units that produce the Tier-3 corpus goldens and have never been EXECUTED
# natively, only compiled. See native/units.sh for why the object set is shared with
# verify.sh rather than trimmed per binary.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
. ./native/units.sh

mkdir -p native/build
native_stage
native_compile $NATIVE_UNITS oracleharness || exit 1

# uistubs.cpp is GENERATED (native/gen-uistubs.py) and includes no engine headers - it
# defines symbols by mangled asm label, so it needs no declarations.
clang++ -c -std=c++14 -O1 -w -o native/build/uistubs.o native/uistubs.cpp

clang++ -o native/build/harness $(native_objs) native/build/oracleharness.o \
    native/build/uistubs.o native/shim/msvcrand.cpp -lz

echo "built native/build/harness"
