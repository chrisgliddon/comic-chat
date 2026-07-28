#!/bin/sh
# units.sh - the shared object list, sourced by build-harness.sh and verify.sh.
#
# ONE object set, several entry points. Keeping two different lists caused a
# whack-a-mole: the dump drivers linked a subset, so every stub deleted from
# nativeglue.cpp after a real engine file started linking became an undefined symbol
# over there, while keeping the stub became a duplicate symbol over here. There is no
# single non-weak arrangement that satisfies two different object sets, so the sets are
# now the same and the drivers differ only in which main() is added.
#
# Cost is link time on a binary that only needs a fraction of it, which is nothing
# compared to maintaining two configurations that disagree.

# Everything except a main(). Ordered roughly by layer.
NATIVE_UNITS="avbdump posedump ojson oracleseed
              nativeglue nativeapp glyphtable glyphtable_cdc stringtable
              vector2d bbox arc spline splinutl traj
              dib avbfile avatar backdrop avatario textpose
              format fonts balloon panel pageview
              chatdoc histent protsupp userinfo doskey urlutil sjis2jis
              ircproto ircsock query ccommon status"

# Entry points, each linked against the set above.
#   oracleharness -> native/build/harness   (corpus replay + every --dump mode)
#   avbmain       -> native/build/avbdump
#   posemain      -> native/build/posedump
#   glyphmain     -> native/build/glyphcheck (standalone: needs no engine object)

NATIVE_CXXFLAGS="-std=c++14 -O1 -w -Wno-error=non-pod-varargs -fms-extensions -DORACLE_HARNESS -I native/shim -I artifacts/inc"

# Stages sources and symlinks the out-of-tree drivers and shim .cpp files into it.
native_stage() {
    ./native/stage.sh > /dev/null
    for c in avbmain posemain glyphmain nativeglue nativeapp; do
        ln -sf "$PWD/native/$c.cpp" "native/stage/$c.cpp"
    done
    for c in glyphtable glyphtable_cdc stringtable; do
        ln -sf "$PWD/native/shim/$c.cpp" "native/stage/$c.cpp"
    done
}

# Compiles a list of units, reporting the first failure with its errors.
native_compile() {
    for u in "$@"; do
        if ! clang++ -c $NATIVE_CXXFLAGS -o "native/build/$u.o" "native/stage/$u.cpp" 2> "native/build/$u.log"; then
            echo "FAILED to compile $u:"
            grep 'error:' "native/build/$u.log" | head -5
            return 1
        fi
    done
    return 0
}

# Expands NATIVE_UNITS to object paths. Never use a *.o glob: native/build also holds
# the drivers and scratch probes, each with its own main() or its own theApp.
native_objs() {
    for u in $NATIVE_UNITS; do printf 'native/build/%s.o ' "$u"; done
}
