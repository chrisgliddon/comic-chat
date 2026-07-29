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
              ircproto ircsock query ccommon status bodycam
              render cgblit cgdraw session asyncsocket msgmap uimaps resources cgsurface"

# Entry points, each linked against the set above.
#   oracleharness -> native/build/harness   (corpus replay + every --dump mode)
#   avbmain       -> native/build/avbdump
#   posemain      -> native/build/posedump
#   glyphmain     -> native/build/glyphcheck (standalone: needs no engine object)
#   sessionmain   -> native/build/sessioncheck (the app's own path, without AppKit)
#   ircmain       -> native/build/irccheck     (connects to a real IRC server)

NATIVE_FRAMEWORKS="-framework ApplicationServices -framework CoreText -framework CoreFoundation"

NATIVE_CXXFLAGS="-std=c++14 -O1 -w -Wno-error=non-pod-varargs -fms-extensions -DORACLE_HARNESS -I native/shim -I artifacts/inc"

# Stages sources and symlinks the out-of-tree drivers and shim .cpp files into it.
native_stage() {
    ./native/stage.sh > /dev/null
    for c in avbmain posemain glyphmain sessionmain ircmain nativeglue nativeapp; do
        ln -sf "$PWD/native/$c.cpp" "native/stage/$c.cpp"
    done
    ln -sf "$PWD/native/render.cpp"  "native/stage/render.cpp"
    ln -sf "$PWD/native/session.cpp" "native/stage/session.cpp"
    ln -sf "$PWD/native/session.h"   "native/stage/session.h"
    ln -sf "$PWD/native/render.h"   "native/stage/render.h"
    for c in glyphtable glyphtable_cdc stringtable cgblit cgdraw asyncsocket msgmap uimaps resources cgsurface; do
        ln -sf "$PWD/native/shim/$c.cpp" "native/stage/$c.cpp"
    done
}

# NO HEADER DEPENDENCY TRACKING. native_compile rebuilds exactly the units named, so after
# editing anything in native/shim/*.h you must clear native/build/*.o rather than recompile
# the unit you were working on. This is not theoretical: changing CPaintDC's constructor from
# an inline no-op to a real out-of-line definition left bodycam.o holding the old inlined
# empty body, and the self-view then painted into a DC with no context - a blank pane with
# every drawing call apparently succeeding. build-harness.sh and verify.sh compile the whole
# set, so they are unaffected.

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
