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

// Comic pages are laid out top-down in reading order, so the view is NOT flipped: the
// renderer installs its own transform per page and expects a bottom-up context, which is
// what NSView gives by default.
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
@property (assign) unsigned short selfAvatar;
@end

@implementation ChatWindowController

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

    self.sayField = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 10, 680, 26)];
    [self.sayField setPlaceholderString:@"Say something…"];
    [self.sayField setDelegate:self];
    [self.sayField setAutoresizingMask:NSViewWidthSizable];
    [content addSubview:self.sayField];

    self.scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(10, 46, 680, 584)];
    [self.scroll setHasVerticalScroller:YES];
    [self.scroll setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [self.scroll setBorderType:NSBezelBorder];

    self.comic = [[ComicView alloc] initWithFrame:NSMakeRect(0, 0, 660, 560)];
    [self.scroll setDocumentView:self.comic];
    [content addSubview:self.scroll];

    [self.window makeFirstResponder:self.sayField];
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

    NativeSessionSay(self.selfAvatar, (const char*)[z bytes]);
    [self.sayField setStringValue:@""];

    [self.comic sizeToContent];
    [self.comic setNeedsDisplay:YES];
    // Newest page is at the bottom of the stack; follow the conversation.
    [self.comic scrollPoint:NSMakePoint(0, 0)];
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
