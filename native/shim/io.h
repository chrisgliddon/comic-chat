// io.h - MSVC's low-level I/O header. The engine uses _findfirst/_findnext to
// enumerate ComicArt, and _access to test for files.
//
// This is a REAL implementation over POSIX, not a stub: backdrop.cpp's
// GetAllBackDropNames walks the art directory with _findfirst, so the native app
// cannot list backdrops without it.

#ifndef NATIVE_SHIM_IO_H
#define NATIVE_SHIM_IO_H

#include "win32types.h"
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

#define _A_NORMAL   0x00
#define _A_RDONLY   0x01
#define _A_HIDDEN   0x02
#define _A_SYSTEM   0x04
#define _A_SUBDIR   0x10
#define _A_ARCH     0x20

struct _finddata_t {
    unsigned attrib;
    long     time_create;
    long     time_access;
    long     time_write;
    unsigned long size;
    char     name[260];
};

// The handle carries the already-matched entry list. Reading the directory once
// up front and sorting it makes enumeration order deterministic, which matters:
// the Win32 original's order is filesystem-defined, and anything that depends on
// it (backdrop IDs are assigned in enumeration order) would otherwise vary
// between machines.
struct _OracleFindHandle {
    std::vector<std::string> names;
    std::vector<unsigned>    attribs;
    size_t                   pos;
};

static inline void _oracleFillFindData(struct _finddata_t* fd, const std::string& name, unsigned attrib) {
    memset(fd, 0, sizeof(*fd));
    fd->attrib = attrib;
    strncpy(fd->name, name.c_str(), sizeof(fd->name) - 1);
}

static inline long _findfirst(const char* pattern, struct _finddata_t* fd) {
    if (!pattern || !fd) return -1L;
    // Normalise separators before splitting: the split below already tolerates a
    // backslash before the filename, but the DIRECTORY half is handed to opendir, which
    // does not. See NativePath in win32types.h for why this is the shim's job.
    std::string p(pattern);
    for (size_t i = 0; i < p.size(); i++) if (p[i] == '\\') p[i] = '/';
    size_t slash = p.find_last_of("/");
    std::string dir  = (slash == std::string::npos) ? "." : p.substr(0, slash);
    std::string glob = (slash == std::string::npos) ? p   : p.substr(slash + 1);
    if (dir.empty()) dir = "/";

    DIR* d = opendir(dir.c_str());
    if (!d) return -1L;

    _OracleFindHandle* h = new _OracleFindHandle();
    h->pos = 0;
    struct dirent* e;
    while ((e = readdir(d)) != 0) {
        if (fnmatch(glob.c_str(), e->d_name, FNM_CASEFOLD) != 0) continue;
        std::string full = dir + "/" + e->d_name;
        struct stat st;
        unsigned attrib = _A_NORMAL;
        if (stat(full.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) attrib |= _A_SUBDIR;
            if (!(st.st_mode & S_IWUSR)) attrib |= _A_RDONLY;
        }
        h->names.push_back(e->d_name);
        h->attribs.push_back(attrib);
    }
    closedir(d);

    if (h->names.empty()) { delete h; return -1L; }

    // Sort for determinism (see the struct comment).
    for (size_t i = 1; i < h->names.size(); i++) {
        for (size_t j = i; j > 0 && h->names[j] < h->names[j - 1]; j--) {
            std::swap(h->names[j], h->names[j - 1]);
            std::swap(h->attribs[j], h->attribs[j - 1]);
        }
    }

    _oracleFillFindData(fd, h->names[0], h->attribs[0]);
    h->pos = 1;
    return (long)(intptr_t)h;
}

static inline int _findnext(long handle, struct _finddata_t* fd) {
    if (handle == -1L || !fd) return -1;
    _OracleFindHandle* h = (_OracleFindHandle*)(intptr_t)handle;
    if (h->pos >= h->names.size()) return -1;
    _oracleFillFindData(fd, h->names[h->pos], h->attribs[h->pos]);
    h->pos++;
    return 0;
}

static inline int _findclose(long handle) {
    if (handle == -1L) return -1;
    delete (_OracleFindHandle*)(intptr_t)handle;
    return 0;
}

// ---------------------------------------------------------------------------
// The Win32 FindFirstFile family, over the same _findfirst machinery above.
//
// Provided so that shared code - oracle/harness/avbdump.cpp is compiled for both
// Windows and macOS - can use one enumeration API rather than being
// platform-conditional. Same sorted, deterministic order.
// ---------------------------------------------------------------------------
typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow;
    DWORD dwReserved0, dwReserved1;
    char  cFileName[260];
    char  cAlternateFileName[14];
} WIN32_FIND_DATAA, WIN32_FIND_DATA, *LPWIN32_FIND_DATA;

static inline void _oracleFindDataToWin32(const struct _finddata_t* fd, WIN32_FIND_DATA* wfd) {
    memset(wfd, 0, sizeof(*wfd));
    // _A_SUBDIR and FILE_ATTRIBUTE_DIRECTORY are different constants; translate
    // rather than copying, or a directory would look like a normal file.
    wfd->dwFileAttributes = (fd->attrib & _A_SUBDIR) ? FILE_ATTRIBUTE_DIRECTORY
                                                     : FILE_ATTRIBUTE_NORMAL;
    if (fd->attrib & _A_RDONLY) wfd->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
    strncpy(wfd->cFileName, fd->name, sizeof(wfd->cFileName) - 1);
}

static inline HANDLE FindFirstFile(const char* pattern, WIN32_FIND_DATA* wfd) {
    if (!wfd) return INVALID_HANDLE_VALUE;
    struct _finddata_t fd;
    long h = _findfirst(pattern, &fd);
    if (h == -1L) return INVALID_HANDLE_VALUE;
    _oracleFindDataToWin32(&fd, wfd);
    return (HANDLE)(intptr_t)h;
}

static inline BOOL FindNextFile(HANDLE h, WIN32_FIND_DATA* wfd) {
    if (h == INVALID_HANDLE_VALUE || !wfd) return FALSE;
    struct _finddata_t fd;
    if (_findnext((long)(intptr_t)h, &fd) != 0) return FALSE;
    _oracleFindDataToWin32(&fd, wfd);
    return TRUE;
}

static inline BOOL FindClose(HANDLE h) {
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    return _findclose((long)(intptr_t)h) == 0;
}

#ifndef _access
#define _access access
#endif
#define _unlink unlink
#define _read   read
#define _write  write
#define _close  close
#define _lseek  lseek

#endif // NATIVE_SHIM_IO_H
