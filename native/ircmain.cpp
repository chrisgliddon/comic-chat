// ircmain.cpp - connect to a real IRC server and report what the engine does with it.
//
//     irccheck <server> <port> <nick> [#channel] [seconds]
//
// A headless IRC client built on native/session.h. It exists so "can it connect to real IRC
// servers" is a question with a testable answer rather than something demonstrated by
// clicking around an app - and so a protocol failure shows up as protocol output instead of
// an empty window.
//
// It drives the main run loop, which is what delivers socket readability to
// CIrcSocket::OnReceive (see native/shim/asyncsocket.cpp). The engine's own state machine
// does the rest: registration, PING/PONG, JOIN, and turning PRIVMSGs into comic panels.

#include "stdafx.h"
#include "session.h"
#include "render.h"

#include "chat.h"
#include "userinfo.h"
#include "vector2d.h"
#include "traj.h"
#include "spline.h"
#include "bbox.h"
#include "pe.h"
#include "dib.h"
#include "avbfile.h"
#include "avatar.h"
#include "balloon.h"
class CPageView;
#include "backdrop.h"
#include "panel.h"

#include <CoreFoundation/CoreFoundation.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A crash here is driven by network input from a run-loop callback, and attaching a debugger
// changes the timing enough that the connection often does not even complete - so the process
// has to report its own backtrace.
static void CrashHandler(int sig) {
    void* frames[32];
    int n = backtrace(frames, 32);
    fprintf(stderr, "\n*** signal %d ***\n", sig);
    char** syms = backtrace_symbols(frames, n);
    if (syms) {
        for (int i = 0; i < n; i++) fprintf(stderr, "  %s\n", syms[i]);
        free(syms);
    }
    fflush(stderr);
    _exit(128 + sig);
}

int main(int argc, char** argv) {
    signal(SIGSEGV, CrashHandler);
    signal(SIGBUS,  CrashHandler);
    signal(SIGILL,  CrashHandler);
    signal(SIGTRAP, CrashHandler);

    if (argc < 4) {
        fprintf(stderr, "usage: irccheck <server> <port> <nick> [#channel] [seconds]\n");
        return 2;
    }
    const char* server  = argv[1];
    int         port    = atoi(argv[2]);
    const char* nick    = argv[3];
    const char* channel = (argc >= 5) ? argv[4] : 0;
    double      budget  = (argc >= 6) ? atof(argv[5]) : 25.0;

    const char* tree = getenv("COMIC_CHAT_TREE");
    if (!tree) tree = "v2.5-beta-1-modern";
    if (!NativeSessionStart(tree)) {
        fprintf(stderr, "irccheck: engine did not start\n");
        return 1;
    }

    // A local avatar, so an incoming message has something to be drawn as.
    unsigned short me = NativeSessionAddSpeaker("bolo", nick);
    NativeSessionSetSelf(me);

    printf("connecting to %s:%d as %s ...\n", server, port, nick);
    if (!NativeSessionConnect(server, port, nick, channel)) {
        fprintf(stderr, "irccheck: connect could not be started\n");
        return 1;
    }

    // Run the loop in slices, reporting status transitions. The engine registers itself on
    // OnConnect and answers PINGs from OnReceive, so this only has to keep the loop turning.
    int lastStatus = -1;
    bool joined = false;
    double elapsed = 0;
    const double slice = 0.25;
    while (elapsed < budget) {
        NativeSessionRunLoopOnce(slice);
        elapsed += slice;

        int st = NativeSessionConnectionStatus();
        if (st != lastStatus) {
            printf("[%5.1fs] status: %s\n", elapsed, NativeSessionConnectionStatusText());
            lastStatus = st;
        }
        // No explicit join: the engine joins g_enterInfo itself on login (see
        // NativeSessionConnect), which is the path the Windows client uses.
        (void)joined;
    }

    printf("final status: %s\n", NativeSessionConnectionStatusText());
    printf("pages built: %d\n", NativeSessionPageCount());

    const char* outDir = getenv("COMIC_CHAT_RENDER_DIR");
    if (outDir) {
        int pages = NativeSessionPageCount();
        for (int i = 0; i < pages; i++) {
            CPage* p = NativeSessionPageAt(i);
            if (!p) continue;
            char path[1024];
            snprintf(path, sizeof(path), "%s/irc%02d.png", outDir, i);
            if (NativeRenderPageToPNG(p, path)) printf("wrote %s\n", path);
        }
    }
    return NativeSessionConnectionStatus() == 0 ? 1 : 0;
}
