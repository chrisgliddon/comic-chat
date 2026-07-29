// main.mm - the AppKit front end.
//
// New code, not an emulation of CChatView. The engine below it is the original 1996 C++,
// reached through native/session.h; everything visible here is Cocoa.
//
// The split is deliberate and is what the whole port has been building toward: the engine
// decides what a comic page looks like (panels, poses, balloon shapes, line breaks) and is
// verified byte-for-byte against Windows by 50 frozen goldens, while this file owns windows,
// scrolling, keyboard and text entry - none of which the goldens have any opinion about.

#import <Cocoa/Cocoa.h>

#include "../session.h"
#include "../render.h"

// ---------------------------------------------------------------------------
// The comic view: draws whatever pages the engine has produced.
// ---------------------------------------------------------------------------
@interface ComicView : NSView
@end

@implementation ComicView

- (BOOL)isOpaque { return YES; }

// NOT flipped: the renderer installs its own transform per page and expects a bottom-up
// context. The consequence is that an NSScrollView opens at the BOTTOM of this view, which is
// what made the title panel look missing - it was above the visible area. The controller
// scrolls to the top explicitly after each update.
- (BOOL)isFlipped { return NO; }

// Total height of every page stacked vertically, so the enclosing scroll view knows how far
// it can scroll as the conversation grows.
- (NSSize)contentSize {
    int pages = NativeSessionPageCount();
    CGFloat w = 0, h = 0;
    const CGFloat gap = 12;
    for (int i = 0; i < pages; i++) {
        CPage* p = NativeSessionPageAt(i);
        if (!p) continue;
        int pw = 0, ph = 0;
        NativeRenderPageSize(p, &pw, &ph);
        if (pw > w) w = pw;
        h += ph + gap;
    }
    if (w < 400) w = 400;
    if (h < 300) h = 300;
    return NSMakeSize(w, h);
}

- (void)sizeToContent {
    NSSize s = [self contentSize];
    [self setFrameSize:s];
}

- (void)drawRect:(NSRect)dirty {
    CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] CGContext];

    [[NSColor colorWithWhite:0.85 alpha:1.0] set];
    NSRectFill(dirty);

    int pages = NativeSessionPageCount();
    if (pages == 0) {
        NSString* msg = @"Type below to start a comic.";
        NSDictionary* attrs = @{
            NSFontAttributeName: [NSFont systemFontOfSize:14],
            NSForegroundColorAttributeName: [NSColor darkGrayColor]
        };
        NSSize sz = [msg sizeWithAttributes:attrs];
        [msg drawAtPoint:NSMakePoint((self.bounds.size.width - sz.width) / 2,
                                     self.bounds.size.height / 2)
          withAttributes:attrs];
        return;
    }

    // Pages stack downward in reading order. The view is bottom-up, so page 0 (oldest) is
    // drawn at the TOP, which means walking y downward from the top of the view.
    const CGFloat gap = 12;
    CGFloat y = self.bounds.size.height;
    for (int i = 0; i < pages; i++) {
        CPage* p = NativeSessionPageAt(i);
        if (!p) continue;
        int pw = 0, ph = 0;
        NativeRenderPageSize(p, &pw, &ph);
        y -= ph;

        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, 0, y);
        NativeRenderPage(p, ctx);
        CGContextRestoreGState(ctx);

        y -= gap;
    }
}
@end

// ---------------------------------------------------------------------------
// Window controller: comic view above, say line below.
// ---------------------------------------------------------------------------
@interface ChatWindowController : NSObject <NSTextFieldDelegate>
@property (strong) NSWindow*     window;
@property (strong) ComicView*    comic;
@property (strong) NSScrollView* scroll;
@property (strong) NSTextField*  sayField;
@property (strong) NSTextField*  serverField;
@property (strong) NSTextField*  nickField;
@property (strong) NSTextField*  roomField;
@property (strong) NSButton*     connectButton;
@property (strong) NSTextField*  statusLabel;
@property (assign) unsigned short selfAvatar;
@property (strong) NSString* channel;
@property (assign) int lastPageCount;
@property (assign) int lastStatus;
@end

@implementation ChatWindowController

// Redraws and keeps the newest page in view. Page 0 is drawn at the TOP of an unflipped view,
// so "scroll to the top" means the maximum y.
- (void)refreshComic {
    [self.comic sizeToContent];
    [self.comic setNeedsDisplay:YES];
    NSRect docRect = [self.comic frame];
    NSRect visible = [[self.scroll contentView] documentVisibleRect];
    [self.comic scrollPoint:NSMakePoint(0, NSMaxY(docRect) - visible.size.height)];
    [self updateTitle];
}

- (void)connectPressed:(id)sender {
    if (NativeSessionConnectionStatus() != 0 /*CX_DISCONNECTED*/) return;   // already up
    NSString* srv = [self.serverField stringValue];
    NSString* nk  = [self.nickField stringValue];
    NSString* ch  = [self.roomField stringValue];
    if (!srv.length || !nk.length) return;
    self.channel = ch;
    [self.statusLabel setStringValue:@"connecting…"];
    if (!NativeSessionConnect([srv UTF8String], 6667, [nk UTF8String],
                              ch.length ? [ch UTF8String] : NULL)) {
        // Reported in the bar, not in a dialog: the failure belongs next to the fields that
        // caused it.
        [self.statusLabel setStringValue:@"failed"];
    }
}

- (void)updateTitle {
    const char* st = NativeSessionConnectionStatusText();
    NSString* t = self.channel.length
        ? [NSString stringWithFormat:@"Comic Chat - %@ (%s)", self.channel, st]
        : [NSString stringWithFormat:@"Comic Chat (%s)", st];
    [self.window setTitle:t];
    [self.statusLabel setStringValue:@(st)];
    BOOL connected = NativeSessionConnectionStatus() != 0;
    [self.connectButton setEnabled:!connected];
    [self.serverField setEnabled:!connected];
    [self.nickField setEnabled:!connected];
    [self.roomField setEnabled:!connected];
}

// The engine turns incoming IRC messages into pages from a run-loop callback, which repaints
// nothing by itself. A timer is the simplest correct bridge: it redraws only when the page
// count or the connection status has actually changed, so an idle connection costs nothing.
- (void)tick:(NSTimer*)t {
    int pages = NativeSessionPageCount();
    int status = NativeSessionConnectionStatus();
    if (pages != self.lastPageCount || status != self.lastStatus) {
        self.lastPageCount = pages;
        self.lastStatus = status;
        [self refreshComic];
    }
}

- (instancetype)init {
    self = [super init];
    if (!self) return nil;

    NSRect frame = NSMakeRect(0, 0, 700, 640);
    self.window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [self.window setTitle:@"Comic Chat"];
    [self.window center];

    NSView* content = [self.window contentView];

    // The CONNECT BAR, inside the window rather than a separate OS dialog. Comic Chat put
    // connecting inside its own frame, and a modal alert owned by the window server is the wrong
    // shape for it: it cannot show live connection status, and it cannot be used again to change
    // rooms without re-presenting a dialog. This is a strip along the top of the window that
    // stays available and reports what the connection is doing.
    CGFloat barY = 600;
    NSTextField* (^label)(NSString*, CGFloat, CGFloat) =
        ^NSTextField*(NSString* t, CGFloat x, CGFloat w) {
            NSTextField* l = [NSTextField labelWithString:t];
            [l setFrame:NSMakeRect(x, barY + 4, w, 18)];
            [l setAlignment:NSTextAlignmentRight];
            [l setAutoresizingMask:NSViewMinYMargin];
            [content addSubview:l];
            return l;
        };
    NSTextField* (^field)(NSString*, CGFloat, CGFloat) =
        ^NSTextField*(NSString* v, CGFloat x, CGFloat w) {
            NSTextField* f = [[NSTextField alloc] initWithFrame:NSMakeRect(x, barY, w, 22)];
            [f setStringValue:v];
            [f setFont:[NSFont systemFontOfSize:11]];
            [f setAutoresizingMask:NSViewMinYMargin];
            [content addSubview:f];
            return f;
        };

    // Defaults are overridable from the environment, which is in character: Comic Chat was an
    // ActiveX document launched from web pages with the room in the URL, so being driveable from
    // outside is how it was meant to work - and it makes the app testable without UI automation.
    const char* envSrv  = getenv("COMIC_CHAT_SERVER");
    const char* envNick = getenv("COMIC_CHAT_NICK");
    const char* envChan = getenv("COMIC_CHAT_CHANNEL");

    label(@"Server:", 6, 46);
    self.serverField = field(envSrv ? @(envSrv) : @"irc.libera.chat", 54, 150);
    label(@"Nick:", 206, 34);
    self.nickField = field(envNick ? @(envNick)
                                   : [NSString stringWithFormat:@"comicchat%d", (int)(getpid() % 9000 + 1000)],
                           242, 110);
    label(@"Room:", 356, 40);
    self.roomField = field(envChan ? @(envChan) : @"#comicchat", 398, 130);

    self.connectButton = [[NSButton alloc] initWithFrame:NSMakeRect(534, barY - 2, 92, 26)];
    [self.connectButton setTitle:@"Connect"];
    [self.connectButton setBezelStyle:NSBezelStyleRounded];
    [self.connectButton setTarget:self];
    [self.connectButton setAction:@selector(connectPressed:)];
    [self.connectButton setAutoresizingMask:NSViewMinYMargin];
    [content addSubview:self.connectButton];

    self.statusLabel = [NSTextField labelWithString:@"offline"];
    [self.statusLabel setFrame:NSMakeRect(630, barY + 4, 64, 18)];
    [self.statusLabel setFont:[NSFont systemFontOfSize:11]];
    [self.statusLabel setTextColor:[NSColor secondaryLabelColor]];
    [self.statusLabel setAutoresizingMask:(NSViewMinYMargin | NSViewMinXMargin)];
    [content addSubview:self.statusLabel];

    self.sayField = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 10, 680, 26)];
    [self.sayField setPlaceholderString:@"Say something…"];
    [self.sayField setDelegate:self];
    [self.sayField setAutoresizingMask:NSViewWidthSizable];
    [content addSubview:self.sayField];

    self.scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(10, 46, 680, 548)];
    [self.scroll setHasVerticalScroller:YES];
    [self.scroll setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [self.scroll setBorderType:NSBezelBorder];

    self.comic = [[ComicView alloc] initWithFrame:NSMakeRect(0, 0, 660, 560)];
    [self.scroll setDocumentView:self.comic];
    [content addSubview:self.scroll];

    [self.window makeFirstResponder:self.sayField];
    if (envSrv && envNick && envChan)
        [self performSelector:@selector(connectPressed:) withObject:nil afterDelay:0.2];
    self.lastPageCount = -1;
    self.lastStatus = -1;
    [NSTimer scheduledTimerWithTimeInterval:0.25 target:self selector:@selector(tick:)
                                   userInfo:nil repeats:YES];
    return self;
}

// Enter in the say field: hand the line to the engine, then redraw.
- (void)controlTextDidEndEditing:(NSNotification*)note {
    NSNumber* reason = [[note userInfo] objectForKey:@"NSTextMovement"];
    if (reason && [reason intValue] != NSReturnTextMovement) return;

    NSString* text = [self.sayField stringValue];
    if ([text length] == 0) return;

    // The engine works in CP-1252, which is what the frozen glyph table is indexed by.
    // Characters outside it are dropped rather than mangled - lossy is better than
    // measuring a byte the table has no advance for, which aborts by design.
    NSData* cp = [text dataUsingEncoding:NSWindowsCP1250StringEncoding allowLossyConversion:YES];
    cp = [text dataUsingEncoding:NSWindowsCP1252StringEncoding allowLossyConversion:YES];
    NSMutableData* z = [NSMutableData dataWithData:cp];
    [z appendBytes:"\0" length:1];

    // Connected: transmit, and let the engine put the line in the local comic as it sends -
    // which is what the Windows client does. Not connected: local composition only.
    if (NativeSessionConnectionStatus() != 0 /*CX_DISCONNECTED*/)
        NativeSessionSendToChannel((const char*)[z bytes]);
    else
        NativeSessionSay(self.selfAvatar, (const char*)[z bytes]);
    [self.sayField setStringValue:@""];

    [self refreshComic];
    [self.window makeFirstResponder:self.sayField];
}
@end

// ---------------------------------------------------------------------------
// App delegate
// ---------------------------------------------------------------------------
@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) ChatWindowController* controller;
@end

@implementation AppDelegate

// Where ComicArt lives. Inside a bundle it is Resources/; from the build tree it is the
// engine directory. Tried in that order so a packaged app never depends on the source tree.
static const char* FindTreeDir(void) {
    static char buf[4096];
    NSString* res = [[NSBundle mainBundle] resourcePath];
    if (res) {
        NSString* art = [res stringByAppendingPathComponent:@"ComicArt"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:art]) {
            strncpy(buf, [res fileSystemRepresentation], sizeof(buf) - 1);
            return buf;
        }
    }
    const char* candidates[] = { "v2.5-beta-1-modern", "../v2.5-beta-1-modern", ".", 0 };
    for (int i = 0; candidates[i]; i++) {
        NSString* c = [NSString stringWithUTF8String:candidates[i]];
        NSString* art = [c stringByAppendingPathComponent:@"ComicArt"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:art]) {
            strncpy(buf, candidates[i], sizeof(buf) - 1);
            return buf;
        }
    }
    return ".";
}

- (void)applicationDidFinishLaunching:(NSNotification*)note {
    const char* tree = FindTreeDir();

    if (!NativeSessionStart(tree)) {
        NSAlert* a = [[NSAlert alloc] init];
        [a setMessageText:@"Comic Chat could not start"];
        [a setInformativeText:[NSString stringWithFormat:
            @"The engine needs its art and data files. Looked for ComicArt in:\n%s\n\n"
             "glyphs.json and strings.json must also be present.", tree]];
        [a runModal];
        [NSApp terminate:nil];
        return;
    }

    self.controller = [[ChatWindowController alloc] init];

    // Two speakers so the panel logic has someone to talk to; the real participant list
    // arrives with IRC.
    unsigned short me = NativeSessionAddSpeaker("bolo", "you");
    NativeSessionAddSpeaker("anna", "anna");
    NativeSessionSetSelf(me);
    self.controller.selfAvatar = me;

    [self.controller.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app { return YES; }

@end

// A minimal menu bar. Without one, Cmd-Q does not work and the app looks broken in a way
// that has nothing to do with the port.
static void InstallMenuBar(void) {
    NSMenu* bar = [[NSMenu alloc] init];
    NSMenuItem* appItem = [[NSMenuItem alloc] init];
    [bar addItem:appItem];
    NSMenu* appMenu = [[NSMenu alloc] init];
    [appMenu addItemWithTitle:@"About Comic Chat" action:nil keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:@"Quit Comic Chat"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    [appItem setSubmenu:appMenu];

    NSMenuItem* editItem = [[NSMenuItem alloc] init];
    [bar addItem:editItem];
    NSMenu* edit = [[NSMenu alloc] initWithTitle:@"Edit"];
    [edit addItemWithTitle:@"Cut"   action:@selector(cut:)       keyEquivalent:@"x"];
    [edit addItemWithTitle:@"Copy"  action:@selector(copy:)      keyEquivalent:@"c"];
    [edit addItemWithTitle:@"Paste" action:@selector(paste:)     keyEquivalent:@"v"];
    [edit addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
    [editItem setSubmenu:edit];

    [NSApp setMainMenu:bar];
}

int main(int argc, const char** argv) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        InstallMenuBar();
        AppDelegate* d = [[AppDelegate alloc] init];
        [NSApp setDelegate:d];
        [NSApp run];
    }
    return 0;
}
