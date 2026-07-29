// afxres.h - MFC's standard command and window IDs.
//
// These were never needed before the message map became real: every BEGIN_MESSAGE_MAP body
// compiled away, so the one place they appear - inside map entries and in
// CMainFrame::OnCreate's docking calls - never had to resolve. Now
// `ON_COMMAND(ID_FILE_PRINT, CView::OnFilePrint)` in pageview.cpp does.
//
// The engine's own resource.h holds its custom IDs (ID_EDIT_DELETE 32889,
// ID_VIEW_TOOLBAR_MAIN 32902, and so on) and deliberately does NOT define these; in the
// original build they arrived from MFC's afxres.h. Values below are MFC's real ones. They
// matter beyond compiling: menu and toolbar resources in chat.rc reference them by number,
// so a wrong value here is a menu item wired to nothing.
//
// Each is guarded, so if a resource header ever defines one first, that wins rather than
// silently redefining.

#ifndef NATIVE_AFXRES_H
#define NATIVE_AFXRES_H

// A separator in a toolbar or status bar. Load-bearing for mainfrm.cpp's `indicators`
// array, whose first element is ID_SEPARATOR.
#ifndef ID_SEPARATOR
#define ID_SEPARATOR                    0
#endif

// --- File menu -------------------------------------------------------------
#ifndef ID_FILE_NEW
#define ID_FILE_NEW                     0xE100
#endif
#ifndef ID_FILE_OPEN
#define ID_FILE_OPEN                    0xE101
#endif
#ifndef ID_FILE_CLOSE
#define ID_FILE_CLOSE                   0xE102
#endif
#ifndef ID_FILE_SAVE
#define ID_FILE_SAVE                    0xE103
#endif
#ifndef ID_FILE_SAVE_AS
#define ID_FILE_SAVE_AS                 0xE104
#endif
#ifndef ID_FILE_PAGE_SETUP
#define ID_FILE_PAGE_SETUP              0xE105
#endif
#ifndef ID_FILE_PRINT_SETUP
#define ID_FILE_PRINT_SETUP             0xE106
#endif
#ifndef ID_FILE_PRINT
#define ID_FILE_PRINT                   0xE107
#endif
#ifndef ID_FILE_PRINT_DIRECT
#define ID_FILE_PRINT_DIRECT            0xE108
#endif
#ifndef ID_FILE_PRINT_PREVIEW
#define ID_FILE_PRINT_PREVIEW           0xE109
#endif
#ifndef ID_FILE_UPDATE
#define ID_FILE_UPDATE                  0xE10A
#endif
#ifndef ID_FILE_SAVE_COPY_AS
#define ID_FILE_SAVE_COPY_AS            0xE10B
#endif
#ifndef ID_FILE_SEND_MAIL
#define ID_FILE_SEND_MAIL               0xE10C
#endif
#ifndef ID_FILE_MRU_FIRST
#define ID_FILE_MRU_FIRST               0xE110
#define ID_FILE_MRU_FILE1               0xE110
#define ID_FILE_MRU_FILE2               0xE111
#define ID_FILE_MRU_FILE3               0xE112
#define ID_FILE_MRU_FILE4               0xE113
#define ID_FILE_MRU_LAST                0xE11F
#endif

// --- Edit menu -------------------------------------------------------------
// Note ID_EDIT_SELECT_ALL (MFC) is a different id from the engine's own
// ID_EDIT_SELECTALL (resource.h, 32890). chatdoc.cpp's map uses the engine's.
#ifndef ID_EDIT_CLEAR
#define ID_EDIT_CLEAR                   0xE120
#endif
#ifndef ID_EDIT_CLEAR_ALL
#define ID_EDIT_CLEAR_ALL               0xE121
#endif
#ifndef ID_EDIT_COPY
#define ID_EDIT_COPY                    0xE122
#endif
#ifndef ID_EDIT_CUT
#define ID_EDIT_CUT                     0xE123
#endif
#ifndef ID_EDIT_FIND
#define ID_EDIT_FIND                    0xE124
#endif
#ifndef ID_EDIT_PASTE
#define ID_EDIT_PASTE                   0xE125
#endif
#ifndef ID_EDIT_PASTE_LINK
#define ID_EDIT_PASTE_LINK              0xE126
#endif
#ifndef ID_EDIT_PASTE_SPECIAL
#define ID_EDIT_PASTE_SPECIAL           0xE127
#endif
#ifndef ID_EDIT_REPEAT
#define ID_EDIT_REPEAT                  0xE128
#endif
#ifndef ID_EDIT_REPLACE
#define ID_EDIT_REPLACE                 0xE129
#endif
#ifndef ID_EDIT_SELECT_ALL
#define ID_EDIT_SELECT_ALL              0xE12A
#endif
#ifndef ID_EDIT_UNDO
#define ID_EDIT_UNDO                    0xE12B
#endif
#ifndef ID_EDIT_REDO
#define ID_EDIT_REDO                    0xE12C
#endif

// --- Window and App menus --------------------------------------------------
#ifndef ID_WINDOW_NEW
#define ID_WINDOW_NEW                   0xE130
#define ID_WINDOW_ARRANGE               0xE131
#define ID_WINDOW_CASCADE               0xE132
#define ID_WINDOW_TILE_HORZ             0xE133
#define ID_WINDOW_TILE_VERT             0xE134
#define ID_WINDOW_SPLIT                 0xE135
#endif
#ifndef ID_APP_ABOUT
#define ID_APP_ABOUT                    0xE140
#endif
#ifndef ID_APP_EXIT
#define ID_APP_EXIT                     0xE141
#endif
#ifndef ID_HELP_INDEX
#define ID_HELP_INDEX                   0xE142
#define ID_HELP_FINDER                  0xE143
#define ID_HELP_USING                   0xE144
#define ID_CONTEXT_HELP                 0xE145
#define ID_HELP                         0xE146
#define ID_DEFAULT_HELP                 0xE147
#endif
#ifndef ID_NEXT_PANE
#define ID_NEXT_PANE                    0xE150
#define ID_PREV_PANE                    0xE151
#endif

// --- View menu -------------------------------------------------------------
#ifndef ID_VIEW_TOOLBAR
#define ID_VIEW_TOOLBAR                 0xE800
#endif
#ifndef ID_VIEW_STATUS_BAR
#define ID_VIEW_STATUS_BAR              0xE801
#endif

// --- status bar indicators -------------------------------------------------
#ifndef ID_INDICATOR_EXT
#define ID_INDICATOR_EXT                0xE700
#define ID_INDICATOR_CAPS               0xE701
#define ID_INDICATOR_NUM                0xE702
#define ID_INDICATOR_SCRL               0xE703
#define ID_INDICATOR_OVR                0xE704
#define ID_INDICATOR_REC                0xE705
#define ID_INDICATOR_KANA               0xE706
#endif

// --- framework window ids --------------------------------------------------
// AFX_IDW_DOCKBAR_* are what CMainFrame::OnCreate passes to DockControlBar and
// GetControlBar, so the tab bar docks to the right edge of the frame.
#ifndef AFX_IDW_TOOLBAR
#define AFX_IDW_TOOLBAR                 0xE800
#define AFX_IDW_STATUS_BAR              0xE801
#define AFX_IDW_PREVIEW_BAR             0xE802
#define AFX_IDW_RESIZE_BAR              0xE803
#define AFX_IDW_DOCKBAR_TOP             0xE81B
#define AFX_IDW_DOCKBAR_LEFT            0xE81C
#define AFX_IDW_DOCKBAR_RIGHT           0xE81D
#define AFX_IDW_DOCKBAR_BOTTOM          0xE81E
#define AFX_IDW_DOCKBAR_FLOAT           0xE81F
#define AFX_IDW_PANE_FIRST              0xE900
#define AFX_IDW_PANE_LAST               0xE9FF
#define AFX_IDW_HSCROLL_FIRST           0xEA00
#define AFX_IDW_VSCROLL_FIRST           0xEA10
#define AFX_IDW_SIZE_BOX                0xEA20
#define AFX_IDW_PANE_SAVE               AFX_IDW_PANE_FIRST
#endif

// AFX_IDS_* strings the framework loads. The engine loads its own strings by its own ids;
// these are only the ones MFC itself would ask for.
#ifndef AFX_IDS_APP_TITLE
#define AFX_IDS_APP_TITLE               0xE000
#define AFX_IDS_IDLEMESSAGE             0xE001
#endif

#endif // NATIVE_AFXRES_H
