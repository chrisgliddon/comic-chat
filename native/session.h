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

// --- IRC ---------------------------------------------------------------------------
// Starts a connection. Returns true if the attempt is UNDER WAY - the socket is
// non-blocking, so success or failure arrives later through the engine's own
// CIrcSocket::OnConnect. Poll NativeSessionConnectionStatus for the outcome.
//
// This bypasses protsupp.cpp's bChatServerConnect, which drives the connection through
// AfxGetMainWnd()->SendMessage and SetTimer - an MFC message loop the native front end does
// not have. The direct sequence it wraps (serverConn.Create then Connect) is the one that
// file's own #if 0'd block uses, so this is the engine's original path rather than a new one.
// `channel` may be NULL. When given it is the room the engine JOINs automatically once
// registration completes - which is how the Windows client works (the room is chosen in the
// connect dialog and CIrcProto::OnLogin joins g_enterInfo). Passing it here rather than
// joining afterwards also avoids the engine sending a JOIN with an EMPTY channel name, which
// a real server answers with "461 Not enough parameters".
bool NativeSessionConnect(const char* server, int port, const char* nickname,
                          const char* channel);

// CX_DISCONNECTED / CX_CONNECTING / CX_NOCHANNEL / CX_INCHANNEL, straight from the engine.
int  NativeSessionConnectionStatus();
const char* NativeSessionConnectionStatusText();

// Joins a channel once connected.
void NativeSessionJoin(const char* channel);

// Sends a line to the current channel, so remote users see it. Distinct from
// NativeSessionSay, which only adds a line to the local comic.
void NativeSessionSendToChannel(const char* text);

// Pumps nothing: the run loop delivers socket events (see native/shim/asyncsocket.cpp).
// Provided so a non-AppKit caller can drive a connection for testing.
void NativeSessionRunLoopOnce(double seconds);

// --- the original UI's own windows ------------------------------------------
//
// A PANE is one of the engine's own CWnd-derived windows, presented to the host as an opaque
// handle. The host gives it a surface, a size and events; everything it draws and every
// decision it makes is the original code's. The self-view pane is CBodyCam, so its character
// rendering and its whole emotion wheel - hit-testing, the live re-pose while dragging, the
// expression icons - are bodycam.cpp, unchanged.
//
// Opaque on purpose, and it is the same reason this header exists at all: the front end must
// not need Win32 or MFC types. Handing out a CWnd* would drag the message map, CPoint, LPARAM
// and the WM_ constants into Objective-C++, where <sys/param.h>'s MAX/MIN macros and AppKit's
// own names start colliding with the engine's.
typedef void* NativePane;

// The self-view and emotion-wheel pane. NULL before NativeSessionStart.
NativePane NativePaneSelfView(void);

// hostView is passed back to the invalidation hook (see NativeSetInvalidateHook in render.h)
// when the engine asks for a repaint from inside its own code, which CBodyCam::RefreshBody
// does after an expression changes.
void NativePaneAttach(NativePane pane, void* hostView, int w, int h);

// A resize. Not bookkeeping: CBodyCam::OnSize recomputes the emotion wheel's radius and icon
// positions from the client width and rebuilds its offscreen bitmap, so a paint without a
// preceding resize uses the constructor's defaults.
void NativePaneResize(NativePane pane, int w, int h);

// Paints the pane into a Core Graphics context by sending it WM_PAINT. The context must be
// bottom-up with no flip applied; the y-downward transform a window's client coordinates
// assume is installed here.
void NativePanePaint(NativePane pane, void* cgContext, int w, int h);

// Mouse input, in CLIENT coordinates: origin top-left, y downward. clickCount 2 or more
// becomes WM_LBUTTONDBLCLK, which is a distinct message in Win32 rather than a modifier on a
// click. Dragging is what drives the emotion wheel.
void NativePaneMouseDown(NativePane pane, int x, int y, int clickCount);
void NativePaneMouseDrag(NativePane pane, int x, int y);
void NativePaneMouseUp(NativePane pane, int x, int y);
void NativePaneMouseMove(NativePane pane, int x, int y);

// Paints a pane to a PNG at the given size. For exercising a pane headlessly, which is how
// the self-view and the emotion wheel were verified before the app had a place to put them.
bool NativePanePaintToPNG(NativePane pane, int w, int h, const char* path);

// Keyboard focus. The engine tests ::GetFocus() == m_hWnd - CBodyCam::OnPaint draws its focus
// rect on exactly that - so the host has to say which pane holds it.
void NativePaneSetFocus(NativePane pane);

#endif // NATIVE_SESSION_H
