// session.h - the engine, wrapped in something an app can call.
//
// The AppKit front end must not know about CChatDoc, theApp, cui, MM_TWIPS DCs or the order
// in which fonts have to be pinned before panel widths. This is the seam: a handful of C
// functions that own the engine's global state and hand back only what the UI needs.
//
// It exists because the setup order is genuinely load-bearing and easy to get wrong. Panel
// dimensions must be pinned BEFORE SetFonts, because UpdateTitleFonts scales the title and
// shout fonts by m_unitWidth. The art directory must be a RELATIVE name joined onto
// m_strBaseDir, because SetArtDir does that join itself and a full path here makes it
// double-join and every avatar load fail. Both are recorded as comments at the site rather
// than left to be rediscovered.
//
// The same setup the oracle harness performs, minus the determinism layer: this is a live
// session, so it does NOT pin the RNG seed or the tick counter.

#ifndef NATIVE_SESSION_H
#define NATIVE_SESSION_H

class CPage;

// Brings the engine up. treeDir is the directory CONTAINING ComicArt (the v2.5 tree, or a
// bundle's Resources). Returns false if the engine could not be initialised - typically a
// missing glyph table or art directory, both of which it reports.
bool NativeSessionStart(const char* treeDir);

// Loads an avatar by name ("bolo") and registers it as a speaker. Returns its avatar id,
// or 0 if the .avb could not be loaded.
unsigned short NativeSessionAddSpeaker(const char* avatarName, const char* nickname);

// Which avatar the local user speaks as.
void NativeSessionSetSelf(unsigned short avatarID);

// Adds a line of dialogue and lets the engine lay it out: emotion detection, pose
// selection, panel and balloon placement. This is CChatDoc::ProcessLine, i.e. exactly the
// path the corpus goldens cover.
void NativeSessionSay(unsigned short avatarID, const char* text);

// The page currently being composed - what the view should draw. NULL before the first line.
CPage* NativeSessionCurrentPage();

// How many pages the conversation has produced, and page n (0 = oldest).
int    NativeSessionPageCount();
CPage* NativeSessionPageAt(int n);

#endif // NATIVE_SESSION_H
