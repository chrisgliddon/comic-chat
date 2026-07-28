// process.h - MSVC's _beginthread family. protsupp.cpp and the socket layer spawn
// worker threads.
//
// Real implementations over pthreads, not stubs: the engine genuinely runs
// background threads (mcithrd.cpp, the download path), and returning failure would
// change control flow rather than merely disable a feature.
//
// _beginthreadex's Win32 signature returns a handle and takes a security attribute
// and flags; those are ignored - macOS has no equivalent and the engine passes NULL/0.
#ifndef NATIVE_SHIM_PROCESS_H
#define NATIVE_SHIM_PROCESS_H

#include "win32types.h"
#include <pthread.h>

typedef unsigned (*_beginthreadex_proc_type)(void*);

namespace {
struct _OracleThreadStart { void (*fn)(void*); void* arg; };
inline void* _oracleThreadTrampoline(void* p) {
    _OracleThreadStart* s = (_OracleThreadStart*)p;
    void (*fn)(void*) = s->fn;
    void* arg = s->arg;
    free(s);
    if (fn) fn(arg);
    return 0;
}
}

// Returns a pthread_t cast to uintptr_t, or -1 on failure, mirroring _beginthread.
inline uintptr_t _beginthread(void (*start)(void*), unsigned, void* arg) {
    _OracleThreadStart* s = (_OracleThreadStart*)malloc(sizeof(*s));
    if (!s) return (uintptr_t)-1;
    s->fn = start; s->arg = arg;
    pthread_t t;
    if (pthread_create(&t, 0, _oracleThreadTrampoline, s) != 0) { free(s); return (uintptr_t)-1; }
    pthread_detach(t);
    return (uintptr_t)t;
}

inline void _endthread() { pthread_exit(0); }

#endif
