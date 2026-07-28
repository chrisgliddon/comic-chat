// posemain.cpp - native macOS driver for the Tier-1 #3 (avatario) and #2
// (avatar-pose) dumps. Milestone 2 of the native port.
//
// Runs the same code as the Windows harness (oracle/harness/posedump.cpp) so the
// output can be diffed against oracle/avatario/avatario.golden.json and
// oracle/avatar/avatar.golden.json.
//
// These two are pure logic: emotion quantisation to and from the wire bytes, and
// the nearest-neighbour pose search over synthetic avatars. No assets, no DC, no
// document. So a clean diff isolates the arithmetic - float-to-double widening, the
// PI value used by the angle metric, and the MSVC RNG - from everything else.

#include "stdafx.h"
#include <stdio.h>
#include <string>

#include "ojson.h"
#include "posedump.h"

static int writeDump(const char* path, const ojson::Value& v) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    std::string out = v.EmitToString();
    fwrite(out.c_str(), 1, out.size(), f);
    fclose(f);
    printf("wrote %s\n", path);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <avatario.json> <avatar.json>\n", argv[0]);
        return 2;
    }
    if (writeDump(argv[1], CaptureAvatario()) != 0) return 1;
    if (writeDump(argv[2], CaptureAvatarPose()) != 0) return 1;
    return 0;
}
