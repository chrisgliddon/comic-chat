// posedump.cpp - Tier-1 #3 and #2 dumps. See posedump.h for why this is a separate
// translation unit shared with the native macOS build.

#include "stdafx.h"
#include <stdio.h>
#include <string.h>

// vector2d.h for PI. RULEBOOK 1.2: PI is tree-dependent, and the avatar angle
// metric uses v2.5's full-precision value - taking it from anywhere else (or from
// M_PI) would shift the nearest-pose search.
#include "vector2d.h"
#include "bbox.h"
#include "pe.h"
#include "dib.h"
#include "avatar.h"
#include "avatario.h"

#include "ojson.h"
#include "posedump.h"

// avatario.cpp forward-declares these locally (lines 66-67) rather than in
// avatario.h, so they are repeated here.
extern void EmotionToBytes(CEmotion &em, BYTE &emotion, BYTE &intensity);
extern void BytesToEmotion(CEmotion &em, BYTE emIndex, BYTE inIndex);

// protsupp.cpp's wire-byte digit conversions, likewise declared locally in
// avatario.cpp rather than in a header.
extern BYTE IndexToByte(BYTE);
extern BYTE ByteToIndex(BYTE);

// ---------------------------------------------------------------------------
// CaptureAvatario — Tier-1 #3 dump: emotion quantization round-trip
// (EmotionToBytes -> BytesToEmotion, avatario.cpp:69-87). Runs a fixed
// probe battery of (emotion, intensity) pairs through the wire encode/decode
// and emits the round-trip data so the TS avatario port has a real oracle
// golden to diff against.
//
// The battery covers every emFloats[] entry at intensity 1.0 (round-trip
// check), an out-of-range emotion (NEUTRAL fallback), and a sweep of
// intensities (truncation behavior). Each entry has:
//   - input.{emotion, intensity}     — the source floats (%.17g)
//   - emVal                          — the table index picked by EmotionToBytes
//   - encoded.{emotion, intensity}   — the wire bytes (IndexToByte outputs)
//   - decoded.{emotion, intensity}   — the floats recovered by BytesToEmotion
//
// NOTE on emFloatsCount: avatario.h declares `extern float emFloats[]` (no
// size) so `sizeof(emFloats)` is illegal from this translation unit. The
// table currently has 18 entries (avatario.cpp:45-64); we hardcode the
// count here. If the table grows, update this constant and the corresponding
// port/src/engine/avatario.ts emFloats[] literal together.
// ---------------------------------------------------------------------------
static const int kEmFloatsCount = 18;
ojson::Value CaptureAvatario() {
    // Probe battery. KEEP IN SYNC with port/src/engine/avatario_dump.ts
    // PROBES[] — both must contain the same inputs in the same order so
    // the TS golden test can diff the dump byte-for-byte.
    struct Probe { double emotion; double intensity; };
    Probe probes[] = {
        // 17 known emotions at intensity 1.0 (cover the entire emFloats[] table).
        { 0.0, 1.0 },                       // NEUTRAL via 0.0
        { EM_HAPPY, 1.0 },                  // HAPPY
        { EM_COY, 1.0 },                    // COY
        { EM_BORED, 1.0 },                  // BORED
        { EM_SCARED, 1.0 },                 // SCARED
        { EM_SAD, 1.0 },                    // SAD
        { EM_ANGRY, 1.0 },                  // ANGRY
        { EM_SHOUT, 1.0 },                  // SHOUT
        { EM_LAUGH, 1.0 },                  // LAUGH
        { EM_NEUTRAL, 1.0 },                // NEUTRAL via EM_NEUTRAL
        { EM_WAVE, 1.0 },                   // WAVE
        { EM_POINTOTHER, 1.0 },             // POINTOTHER
        { EM_POINTSELF, 1.0 },              // POINTSELF
        { EM_DOUBLEPOINT, 1.0 },            // DOUBLEPOINT
        { EM_SHRUG, 1.0 },                  // SHRUG
        { EM_3QRWALK, 1.0 },                // 3QRWALK
        { EM_SIDEWALK, 1.0 },               // SIDEWALK
        { EM_3QFWALK, 1.0 },                // 3QFWALK
        // Out-of-range emotion -> emVal stays at 9 (NEUTRAL).
        { 999.0, 1.0 },                     // out-of-range -> NEUTRAL
        // Intensity scaling
        { EM_HAPPY, 0.0 },                  // intensity=0 -> 0 byte
        { EM_HAPPY, 0.3 },                  // intensity=0.3 -> 3 byte
        { EM_HAPPY, 0.5 },                  // intensity=0.5 -> 5 byte
        { EM_HAPPY, 1.5 },                  // intensity=1.5 -> 15 byte
    };
    int nProbes = sizeof(probes) / sizeof(probes[0]);

    ojson::Value root = ojson::Value::Obj();

    // Dump the emFloats[] table itself for oracle comparison.
    int nFloats = kEmFloatsCount;
    ojson::Value emFloatsArr = ojson::Value::Arr();
    for (int i = 0; i < nFloats; i++) {
        char buf[64];
        sprintf(buf, "%.17g", (double)emFloats[i]);
        emFloatsArr.Push(ojson::Value::Str(buf));
    }
    root.Set("emFloats", emFloatsArr);

    // Run the probe battery through EmotionToBytes -> BytesToEmotion.
    ojson::Value probesArr = ojson::Value::Arr();
    for (int i = 0; i < nProbes; i++) {
        CEmotion emIn((float)probes[i].intensity, (float)probes[i].emotion);
        BYTE eByte, iByte;
        EmotionToBytes(emIn, eByte, iByte);

        // Decode back: pass the RAW table indices (ByteToIndex reverses
        // the wire IndexToByte step the encoder applied).
        CEmotion emOut;
        BytesToEmotion(emOut, ByteToIndex(eByte), ByteToIndex(iByte));

        // Compute emVal the encoder picked — independent re-derivation for
        // the dump's sanity field (matches the port's dumpAvatarioProbes).
        int emVal = 9; // default on no match
        for (int j = 1; j < nFloats; j++) {
            if (emFloats[j] == emIn.m_emotion) {
                emVal = j;
                break;
            }
        }

        ojson::Value entry = ojson::Value::Obj();
        // input
        ojson::Value inObj = ojson::Value::Obj();
        char buf[64];
        sprintf(buf, "%.17g", (double)probes[i].emotion);
        inObj.Set("emotion", ojson::Value::Str(buf));
        sprintf(buf, "%.17g", (double)probes[i].intensity);
        inObj.Set("intensity", ojson::Value::Str(buf));
        entry.Set("input", inObj);
        entry.Set("emVal", ojson::Value::Int(emVal));
        // encoded (wire bytes)
        ojson::Value encObj = ojson::Value::Obj();
        encObj.Set("emotion", ojson::Value::Int((long)eByte));
        encObj.Set("intensity", ojson::Value::Int((long)iByte));
        entry.Set("encoded", encObj);
        // decoded (back to floats)
        ojson::Value decObj = ojson::Value::Obj();
        sprintf(buf, "%.17g", (double)emOut.m_emotion);
        decObj.Set("emotion", ojson::Value::Str(buf));
        sprintf(buf, "%.17g", (double)emOut.m_intensity);
        decObj.Set("intensity", ojson::Value::Str(buf));
        entry.Set("decoded", decObj);

        probesArr.Push(entry);
    }
    root.Set("probes", probesArr);
    return root;
}

// ---------------------------------------------------------------------------
// CaptureAvatarPose — Tier-1 #2 dump: emotion-to-pose selection on
// SYNTHETIC avatars. Constructs CAvatarSimple and CAvatarComplex with
// known bRec/fRec arrays (no real .avb loading — that's Phase 4 avbfile
// work) and runs a probe battery of GetBodyFromEmotion calls. Emits the
// chosen bodyIndex/faceIndex/torsoIndex + the post-state m_last*.
//
// The synthetic data + probe list mirror `port/src/engine/avatar_dump.ts`
// exactly so the TS golden test can diff byte-for-byte. This is a UNIT
// test of the nearest-neighbor selection — it does NOT exercise the real
// avatar-loading path (which is Phase 4 `avbfile.cpp`).
// ---------------------------------------------------------------------------
ojson::Value CaptureAvatarPose() {
    // Synthetic bRec (8 entries — 6 directional + NEUTRAL + WAVE). Identical
    // to the TS dump's SIMPLE_BREC. poseIDs and faceX/faceY are
    // oracle-irrelevant (we only emit integer indices).
    RBODYREC simpleBRec[8] = {
        { 100, (float)EM_HAPPY,  0.5f, 10, 5 },
        { 101, (float)EM_SCARED, 0.5f, 10, 5 },
        { 102, (float)EM_SAD,    0.5f, 10, 5 },
        { 103, (float)EM_ANGRY,  0.5f, 10, 5 },
        { 104, (float)EM_SHOUT,  0.5f, 10, 5 },
        { 105, (float)EM_LAUGH,  0.5f, 10, 5 },
        { 110, (float)EM_NEUTRAL, 0.0f, 10, 5 },
        { 120, (float)EM_WAVE,   1.0f, 10, 5 },
    };
    int simpleN = sizeof(simpleBRec) / sizeof(simpleBRec[0]);

    // Complex avatar synthetic data — matches the TS dump.
    FACEREC complexFRec[7] = {
        { 200, (float)EM_HAPPY,  0.5f, 0, 0, 0, 0, 8, 4 },
        { 201, (float)EM_SCARED, 0.5f, 0, 0, 0, 0, 8, 4 },
        { 202, (float)EM_SAD,    0.5f, 0, 0, 0, 0, 8, 4 },
        { 203, (float)EM_ANGRY,  0.5f, 0, 0, 0, 0, 8, 4 },
        { 204, (float)EM_SHOUT,  0.5f, 0, 0, 0, 0, 8, 4 },
        { 205, (float)EM_LAUGH,  0.5f, 0, 0, 0, 0, 8, 4 },
        { 210, (float)EM_NEUTRAL, 0.0f, 0, 0, 0, 0, 8, 4 },
    };
    BODYREC complexBRec[5] = {
        { 300, (float)EM_HAPPY,  0.5f, 0, 0 },
        { 301, (float)EM_SAD,    0.5f, 0, 0 },
        { 302, (float)EM_ANGRY,  0.5f, 0, 0 },
        { 303, (float)EM_SHOUT,  0.5f, 0, 0 },
        { 310, (float)EM_NEUTRAL, 0.0f, 0, 0 },
    };
    int complexFN = sizeof(complexFRec) / sizeof(complexFRec[0]);
    int complexBN = sizeof(complexBRec) / sizeof(complexBRec[0]);

    // Probe battery — identical to the TS dumpAvatarProbes arrays.
    struct Probe { double emotion; double intensity; };
    Probe simpleProbes[] = {
        { EM_HAPPY, 0.5 },
        { EM_SAD,   0.5 },
        { EM_SHOUT, 0.5 },
        { EM_LAUGH, 0.5 },
        { EM_SCARED, 0.5 },
        { EM_ANGRY, 0.5 },
        { EM_WAVE, 1.0 },
        { 1004.0, 1.0 },  // EM_DOUBLEPOINT sentinel
        { 9999.0, 1.0 },  // unknown sentinel
    };
    int nSimpleProbes = sizeof(simpleProbes) / sizeof(simpleProbes[0]);

    Probe complexProbes[] = {
        { EM_HAPPY, 0.5 },
        { EM_SAD,   0.5 },
        { EM_SHOUT, 0.5 },
        { EM_LAUGH, 0.5 },
        { EM_SCARED, 0.5 },
        { EM_ANGRY, 0.5 },
        { EM_WAVE, 1.0 },
        { 1004.0, 1.0 },  // EM_DOUBLEPOINT sentinel
    };
    int nComplexProbes = sizeof(complexProbes) / sizeof(complexProbes[0]);

    ojson::Value root = ojson::Value::Obj();

    // Simple avatar probes — fresh avatar per probe (m_lastBody resets to -1).
    ojson::Value simpleArr = ojson::Value::Arr();
    for (int i = 0; i < nSimpleProbes; i++) {
        CAvatarSimple av;
        av.bRec = simpleBRec;
        av.m_nBodies = simpleN;
        av.m_lastBody = -1; // redundant (ctor sets it), explicit for clarity
        // Prevent ~CAvatarSimple from freeing our stack bRec.
        av.m_origID = 1;

        CEmotion emIn((float)simpleProbes[i].intensity, (float)simpleProbes[i].emotion);
        CBody *bodyPtr = av.GetBodyFromEmotion(emIn);
        CBodySingle *body = (CBodySingle *)bodyPtr;
        int bodyIndex = body->m_bodyRec - simpleBRec;

        ojson::Value entry = ojson::Value::Obj();
        ojson::Value inObj = ojson::Value::Obj();
        char buf[64];
        sprintf(buf, "%.17g", simpleProbes[i].emotion);
        inObj.Set("emotion", ojson::Value::Str(buf));
        sprintf(buf, "%.17g", simpleProbes[i].intensity);
        inObj.Set("intensity", ojson::Value::Str(buf));
        entry.Set("input", inObj);
        entry.Set("bodyIndex", ojson::Value::Int(bodyIndex));
        entry.Set("m_lastBody", ojson::Value::Int(av.m_lastBody));
        simpleArr.Push(entry);

        delete body; // GetBodyFromEmotion does `new CBodySingle`
    }
    root.Set("simple", simpleArr);

    // Complex avatar probes — fresh avatar per probe.
    ojson::Value complexArr = ojson::Value::Arr();
    for (int i = 0; i < nComplexProbes; i++) {
        CAvatarComplex av;
        av.fRec = complexFRec;
        av.bRec = complexBRec;
        av.nFaces = complexFN;
        av.nTorsos = complexBN;
        av.m_lastFace = -1;
        av.m_lastTorso = -1;
        // Prevent ~CAvatarComplex from freeing our stack fRec/bRec.
        av.m_origID = 1;

        CEmotion emIn((float)complexProbes[i].intensity, (float)complexProbes[i].emotion);
        CBody *bodyPtr = av.GetBodyFromEmotion(emIn);
        CBodyDouble *body = (CBodyDouble *)bodyPtr;
        int faceIndex = body->m_faceRec - complexFRec;
        int torsoIndex = body->m_torsoRec - complexBRec;

        ojson::Value entry = ojson::Value::Obj();
        ojson::Value inObj = ojson::Value::Obj();
        char buf[64];
        sprintf(buf, "%.17g", complexProbes[i].emotion);
        inObj.Set("emotion", ojson::Value::Str(buf));
        sprintf(buf, "%.17g", complexProbes[i].intensity);
        inObj.Set("intensity", ojson::Value::Str(buf));
        entry.Set("input", inObj);
        entry.Set("faceIndex", ojson::Value::Int(faceIndex));
        entry.Set("torsoIndex", ojson::Value::Int(torsoIndex));
        entry.Set("m_lastFace", ojson::Value::Int(av.m_lastFace));
        entry.Set("m_lastTorso", ojson::Value::Int(av.m_lastTorso));
        complexArr.Push(entry);

        delete body; // GetBodyFromEmotion does `new CBodyDouble`
    }
    root.Set("complex", complexArr);

    return root;
}
