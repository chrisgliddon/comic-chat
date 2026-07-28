// posedump.h - Tier-1 #3 (avatario emotion quantisation round-trip) and #2
// (avatar-pose emotion-to-pose selection) dumps.
//
// Extracted from oracleharness.cpp so the native macOS build can run the SAME code
// as the Windows harness, for the reason set out in avbdump.h: a golden match
// between two separate implementations could be coincidence, whereas a match here
// isolates the platform layer as the only variable.
//
// Requires avatario.cpp and avatar.cpp. Needs no DC, no fonts, no asset files and
// no document - which is what makes these the natural milestone 2 after --avb.
#ifndef ORACLE_POSEDUMP_H
#define ORACLE_POSEDUMP_H
#include "ojson.h"
ojson::Value CaptureAvatario();
ojson::Value CaptureAvatarPose();
#endif
