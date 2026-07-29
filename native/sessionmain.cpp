// sessionmain.cpp - drive a session from the command line and report what the engine built.
//
//     sessionmain <treeDir> [outDir]
//
// The app's own path (native/session.h), exercised without AppKit. This exists because
// diagnosing the app through the GUI is slow and imprecise: here the panel/element/body
// structure is printed, so "the title is missing" becomes "the title panel has 2 labels and
// they are at these coordinates" - a fact rather than an impression.
//
// Speaks as alternating avatars, which the GUI cannot yet do: the local user is one speaker,
// so a second avatar only appears when something else supplies its lines. That is exactly
// what IRC will do, and it is worth being able to test the layout before then.

#include "stdafx.h"
#include "session.h"
#include "render.h"

// The engine headers this needs, in the order they demand. balloon.h references CChatApp,
// CBody, CSpline and CTraj without declaring them - the engine .cpp files get those from
// their own include chains, so the chain has to be reproduced rather than trimmed.
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

#include <stdio.h>
#include <string.h>

static void DumpPage(CPage* page, int index) {
    RECT bb;
    page->GetBBox(&bb);
    printf("page %d: bbox=(%d,%d,%d,%d) panels=%d\n", index,
           (int)bb.left, (int)bb.top, (int)bb.right, (int)bb.bottom,
           (int)page->m_panels.GetCount());

    int pn = 0;
    POSITION pos = page->m_panels.GetHeadPosition();
    while (pos) {
        CPanel* panel = (CPanel*)page->m_panels.GetNext(pos);
        int nBalloons = 0, nLabels = 0;
        POSITION ep = panel->m_elements.GetHeadPosition();
        while (ep) {
            CPanelElement* e = (CPanelElement*)panel->m_elements.GetNext(ep);
            if (e->GetType() & PE_BALLOON) nBalloons++; else nLabels++;
        }
        printf("  panel %d: border=%d backdropID=%d mode=%d elements=%d (balloons=%d labels=%d) bodies=%d\n",
               pn++, panel->m_hasBorder ? 1 : 0,
               (int)panel->m_backDrop.m_backID, (int)panel->m_backDrop.m_mode,
               (int)panel->m_elements.GetCount(), nBalloons, nLabels,
               (int)panel->m_bodies.GetCount());

        ep = panel->m_elements.GetHeadPosition();
        while (ep) {
            CPanelElement* e = (CPanelElement*)panel->m_elements.GetNext(ep);
            CLabel* lab = (CLabel*)e;
            printf("      %-8s bbox=(%d,%d,%d,%d) str=\"%.48s\"\n",
                   (e->GetType() & PE_BALLOON) ? "balloon" : "label",
                   lab->m_bbox.Left, lab->m_bbox.Top, lab->m_bbox.Right, lab->m_bbox.Bottom,
                   lab->m_str ? lab->m_str : "(null)");
        }
        POSITION bp = panel->m_bodies.GetHeadPosition();
        while (bp) {
            CBody* b = (CBody*)panel->m_bodies.GetNext(bp);
            printf("      body    avatarID=%d flip=%d bbox=(%d,%d,%d,%d)\n",
                   b->m_avatarID, b->m_flip,
                   b->m_bbox.Left, b->m_bbox.Top, b->m_bbox.Right, b->m_bbox.Bottom);
        }
    }
}

int main(int argc, char** argv) {
    const char* tree = (argc >= 2) ? argv[1] : "v2.5-beta-1-modern";
    const char* outDir = (argc >= 3) ? argv[2] : 0;

    if (!NativeSessionStart(tree)) {
        fprintf(stderr, "sessionmain: could not start the engine\n");
        return 1;
    }

    unsigned short bolo = NativeSessionAddSpeaker("bolo", "bolo");
    unsigned short anna = NativeSessionAddSpeaker("anna", "anna");
    printf("speakers: bolo=%d anna=%d\n", bolo, anna);
    NativeSessionSetSelf(bolo);

    struct { unsigned short who; const char* text; } lines[] = {
        { bolo, "Hello there! How are you?" },
        { anna, "I'm doing great, thanks for asking!" },
        { bolo, "I should be waving when I say hi" },
        { anna, "That's wonderful to hear." },
    };
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
        NativeSessionSay(lines[i].who, lines[i].text);

    // The SELF-VIEW pane, painted by the engine's own CBodyCam::OnPaint. Worth doing here
    // rather than only in the app: it isolates whether the message map and the window paint
    // path work from whether the app has laid the pane out correctly.
    //
    // 256x400 is roughly the proportion the original gives it - the character standing above
    // the emotion wheel (see the screenshots in docs). CBodyCam caches the wheel's geometry
    // from the window width, so the size genuinely changes what it draws.
    if (outDir) {
        CWnd* cam = NativeSessionBodyCamWnd();
        if (!cam) {
            printf("bodycam: none\n");
        } else {
            cam->NativeAttach(0, 256, 400);
            // WM_SIZE first: CacheBullSide computes the wheel's radius and icon positions from
            // the client width, and OnPaint uses those. Painting without it draws the wheel at
            // whatever the constructor's defaults were.
            cam->SendMessage(WM_SIZE, 0, (400 << 16) | 256);
            char path[1024];
            snprintf(path, sizeof(path), "%s/bodycam.png", outDir);
            if (NativeWndPaintToPNG(cam, 256, 400, path))
                printf("bodycam: wrote %s\n", path);
            else
                printf("bodycam: paint failed\n");
        }
    }

    int pages = NativeSessionPageCount();
    printf("pages: %d\n", pages);
    for (int i = 0; i < pages; i++) {
        CPage* p = NativeSessionPageAt(i);
        if (!p) { printf("page %d: NULL\n", i); continue; }
        DumpPage(p, i);
        if (outDir) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/session%02d.png", outDir, i);
            if (NativeRenderPageToPNG(p, path)) printf("  wrote %s\n", path);
        }
    }
    return 0;
}
