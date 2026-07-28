// mfcshim.h - the MFC surface the Comic Chat engine actually uses.
//
// Scope discipline: this implements only what the engine calls, and it is grown
// by compiler error, not by reading MFC's documentation. Anything here that is
// not exercised by a shipped code path is a liability, because it looks like
// support that has never been run.
//
// Semantics to preserve, not merely compile:
//
//  * CString is byte-oriented (MBCS), never wide. The engine does strchr on it,
//    compares bytes, and carries CP-1252 payloads like the 0xA9 in AK_COPYRIGHT.
//  * CString::operator[] out of range and GetAt are used freely; MFC does not
//    bounds-check in release, and the engine relies on reading the NUL.
//  * The Cxxx array classes grow on SetAtGrow/Add and zero-fill the gap.
//  * TRY/CATCH_ALL must catch what AfxThrow* throws AND nothing else, since the
//    engine uses CATCH_ALL as flow control around allocation (avbfile.cpp).
//  * ASSERT compiles OUT. This is not laziness: the engine ships with asserts
//    disabled and several release paths depend on it - ircsock.cpp's ParseIt
//    ASSERTs a pointer it then dereferences anyway, and avbfile's record loaders
//    ASSERT preconditions that malformed files violate. Making ASSERT throw here
//    would change behaviour the oracle has already pinned.

#ifndef NATIVE_SHIM_MFCSHIM_H
#define NATIVE_SHIM_MFCSHIM_H

#include "win32types.h"
#include <string>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdarg.h>
#include <new>

// ---------------------------------------------------------------------------
// Diagnostics. ASSERT/TRACE vanish, matching the shipped release build.
// ---------------------------------------------------------------------------
#define ASSERT(expr)        ((void)0)
#define ASSERT_VALID(p)     ((void)0)
#define VERIFY(expr)        ((void)(expr))
#ifdef NATIVE_SHIM_TRACE
#define TRACE(...)          ((void)fprintf(stderr, __VA_ARGS__))
#else
#define TRACE(...)          ((void)0)
#endif
#define TRACE0(s)           TRACE("%s", s)
#define TRACE1(s, a)        TRACE(s, a)
#define TRACE2(s, a, b)     TRACE(s, a, b)

// ---------------------------------------------------------------------------
// Exceptions. CException is thrown BY POINTER, as MFC does, because the engine's
// CATCH_ALL(e) blocks bind `e` as a pointer and some call DELETE_EXCEPTION.
// ---------------------------------------------------------------------------
class CException {
public:
    virtual ~CException() {}
    virtual const char* What() const { return "CException"; }
};
class CMemoryException : public CException {
public:
    virtual const char* What() const { return "CMemoryException"; }
};
class CFileException : public CException {
public:
    // The engine tests e->m_cause == CFileException::endOfFile as normal control
    // flow when reading rules files, so the enum must carry MFC's values.
    enum Cause {
        none = 0, generic, fileNotFound, badPath, tooManyOpenFiles,
        accessDenied, invalidFile, removeCurrentDir, directoryFull,
        badSeek, hardIO, sharingViolation, lockViolation, diskFull, endOfFile
    };
    int m_cause;
    CFileException(int cause = 0) : m_cause(cause) {}
    virtual const char* What() const { return "CFileException"; }
};
class CArchiveException : public CException {};

// CDataExchange is the DDX cursor passed to DoDataExchange. Dialog data exchange
// never runs in the native build (there are no MFC dialogs), so this exists only
// so the overrides in the engine's dialog headers are declarable.
class CDataExchange {
public:
    BOOL m_bSaveAndValidate;
    CWnd* m_pDlgWnd;
    CDataExchange() : m_bSaveAndValidate(FALSE), m_pDlgWnd(0) {}
};
class CNotSupportedException : public CException {};

inline void AfxThrowMemoryException()       { throw new CMemoryException(); }
inline void AfxThrowFileException(int c = 0){ throw new CFileException(c); }
inline void AfxThrowArchiveException(int)   { throw new CArchiveException(); }
class CUserException : public CException {};
inline void AfxThrowNotSupportedException() { throw new CNotSupportedException(); }
inline void AfxThrowUserException()         { throw new CUserException(); }

#define TRY                 try {
#define CATCH_ALL(e)        } catch (CException* e) { (void)e;
#define END_CATCH_ALL       }
#define CATCH(cls, e)       } catch (cls* e) { (void)e;
#define END_CATCH           }
#define AND_CATCH_ALL(e)    } catch (CException* e) { (void)e;
#define THROW_LAST()        throw
#define DELETE_EXCEPTION(e) do { delete (e); } while (0)

// ---------------------------------------------------------------------------
// CObject - the engine only needs it as a base with a virtual dtor. The
// serialization macros are no-ops: every DECLARE_SERIAL class in the engine that
// this port touches is either not serialized at all or goes through the .ccc text
// codec instead (Tier-1 #10), not CArchive.
// ---------------------------------------------------------------------------
class CArchive;
class CFile;     // defined in gdishim.h, which includes this header
class CString;   // defined below; CArchive's string methods need it

class CObject {
public:
    virtual ~CObject() {}
    virtual void Serialize(CArchive&) {}
};

// CArchive - declared because Serialize overrides name it. Note the ORACLE uses
// CArchive over CMemFile to capture byte-exact MFC serialization on Windows; the
// native build does not serialize through it at all (the .ccc text codec is what
// transcripts use), so this is a type, not an implementation. If a native path
// ever needs real CArchive framing, it has to reproduce MFC's tag/schema bytes -
// do not improvise it here.
class CArchive {
public:
    enum Mode { store = 0, load = 1 };
    CArchive(CFile* f = 0, UINT mode = load) : m_pFile(f), m_nMode(mode) {}
    BOOL IsStoring() const { return m_nMode == store; }
    BOOL IsLoading() const { return m_nMode != store; }
    CFile* GetFile() const { return m_pFile; }
    void Close() {}
    void Flush() {}
    // histent.cpp writes transcript lines through these. The .ccc codec is plain
    // text, so raw bytes are the whole contract - no MFC tag/schema framing is
    // involved and none is emulated (see the class comment).
    void WriteString(LPCTSTR s);
    BOOL ReadString(CString& s);
private:
    CFile* m_pFile;
    UINT m_nMode;
};
#define DECLARE_DYNAMIC(cls)
#define IMPLEMENT_DYNAMIC(cls, base)
#define DECLARE_DYNCREATE(cls)
#define IMPLEMENT_DYNCREATE(cls, base)
#define DECLARE_SERIAL(cls)
#define IMPLEMENT_SERIAL(cls, base, ver)
#define RUNTIME_CLASS(cls)  ((CRuntimeClass*)0)
class CRuntimeClass;

// ---------------------------------------------------------------------------
// CString - a SINGLE char* member, deliberately.
//
// The layout is the contract. The engine passes CString straight into printf-style
// varargs in ~580 places (strQName.Format("%s (%s)", nick, m_fullName)), which is
// formally undefined but works in MFC because a CString IS one pointer to a
// NUL-terminated buffer: the vararg push puts that pointer where %s expects it.
//
// An earlier version of this shim held std::string members. That compiled, but
// every one of those call sites would have pushed a std::string object and %s would
// have read its internals as a char* - garbage or a crash, silently, in 580 places.
// Casting at each site was the alternative; matching MFC layout fixes all of them at
// once and keeps the engine source unedited.
//
// Consequences to respect:
//   * m_pchData must remain the ONLY data member. Adding a second (a length cache,
//     a GetBuffer scratch) breaks every vararg site at a stroke.
//   * It is always heap-allocated and NUL-terminated, never null, so the vararg path
//     can never hand printf a null pointer.
//   * clang's -Wnon-pod-varargs is DefaultError, so the build passes
//     -Wno-error=non-pod-varargs. That flag is only sound BECAUSE of this layout -
//     do not keep it if the layout ever changes.
// ---------------------------------------------------------------------------
class CString {
public:
    CString() : m_pchData(0) { Assign("", 0); }
    CString(const char* s) : m_pchData(0) { Assign(s, s ? strlen(s) : 0); }
    CString(const char* s, int n) : m_pchData(0) { Assign(s, (size_t)(n > 0 ? n : 0)); }
    CString(const CString& o) : m_pchData(0) { Assign(o.m_pchData, strlen(o.m_pchData)); }
    CString(char c, int n) : m_pchData(0) {
        size_t k = (size_t)(n > 0 ? n : 0);
        char* q = (char*)malloc(k + 1);
        memset(q, c, k); q[k] = 0;
        m_pchData = q;
    }
    ~CString() { free(m_pchData); }

    CString& operator=(const CString& o) {
        if (this != &o) Assign(o.m_pchData, strlen(o.m_pchData));
        return *this;
    }
    CString& operator=(const char* s) { Assign(s, s ? strlen(s) : 0); return *this; }
    CString& operator=(char c)        { char b[2]; b[0] = c; b[1] = 0; Assign(b, 1); return *this; }

    operator const char*() const { return m_pchData; }

    int GetLength() const       { return (int)strlen(m_pchData); }
    BOOL IsEmpty() const        { return m_pchData[0] == 0 ? TRUE : FALSE; }
    void Empty()                { Assign("", 0); }
    // Index GetLength() yields the NUL, as MFC does - the engine scans that way.
    char GetAt(int i) const     { int L = GetLength(); return (i >= 0 && i <= L) ? m_pchData[i] : 0; }
    char operator[](int i) const { return GetAt(i); }
    void SetAt(int i, char c)   { if (i >= 0 && i < GetLength()) m_pchData[i] = c; }

    CString& operator+=(const char* s) {
        if (s && *s) {
            size_t a = strlen(m_pchData), b = strlen(s);
            char* q = (char*)malloc(a + b + 1);
            memcpy(q, m_pchData, a); memcpy(q + a, s, b); q[a + b] = 0;
            free(m_pchData); m_pchData = q;
        }
        return *this;
    }
    CString& operator+=(const CString& o) { return operator+=(o.m_pchData); }
    CString& operator+=(char c) { char b[2]; b[0] = c; b[1] = 0; return operator+=(b); }

    int Compare(const char* s) const       { return strcmp(m_pchData, s ? s : ""); }
    int CompareNoCase(const char* s) const { return strcasecmp(m_pchData, s ? s : ""); }

    int Find(char c) const        { const char* p = strchr(m_pchData, c); return p ? (int)(p - m_pchData) : -1; }
    int Find(const char* s) const { const char* p = s ? strstr(m_pchData, s) : 0; return p ? (int)(p - m_pchData) : -1; }
    int ReverseFind(char c) const { const char* p = strrchr(m_pchData, c); return p ? (int)(p - m_pchData) : -1; }

    CString Mid(int start, int n) const {
        int L = GetLength();
        if (start < 0 || start > L || n <= 0) return CString();
        if (start + n > L) n = L - start;
        return CString(m_pchData + start, n);
    }
    CString Mid(int start) const { int L = GetLength(); return Mid(start, L - start); }
    CString Left(int n) const    { return Mid(0, n); }
    CString Right(int n) const   { int L = GetLength(); return n >= L ? *this : Mid(L - n, n); }

    void MakeUpper() { for (char* p = m_pchData; *p; p++) *p = (char)toupper((unsigned char)*p); }
    void MakeLower() { for (char* p = m_pchData; *p; p++) *p = (char)tolower((unsigned char)*p); }
    void TrimRight() { int L = GetLength(); while (L > 0 && isspace((unsigned char)m_pchData[L-1])) m_pchData[--L] = 0; }
    void TrimLeft()  { char* p = m_pchData; while (*p && isspace((unsigned char)*p)) p++; if (p != m_pchData) Assign(p, strlen(p)); }

    void Format(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        char buf[2048];
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n >= 0 && (size_t)n < sizeof(buf)) { Assign(buf, (size_t)n); return; }
        va_list ap2; va_start(ap2, fmt);
        size_t need = (size_t)(n > 0 ? n + 1 : 8192);
        char* big = (char*)malloc(need);
        vsnprintf(big, need, fmt, ap2);
        va_end(ap2);
        Assign(big, strlen(big));
        free(big);
    }

    // Hands back the live buffer grown to minLen. No separate scratch member (see
    // the layout note), so the pointer is valid until the next mutation.
    char* GetBuffer(int minLen) {
        int L = GetLength();
        if (minLen > L) {
            char* q = (char*)malloc((size_t)minLen + 1);
            memcpy(q, m_pchData, (size_t)L);
            memset(q + L, 0, (size_t)(minLen - L) + 1);
            free(m_pchData); m_pchData = q;
        }
        return m_pchData;
    }
    void ReleaseBuffer(int newLen = -1) { if (newLen >= 0) m_pchData[newLen] = 0; }

    // LoadString reads the .rc string table, which a Mach-O binary has no
    // equivalent of. Returns FALSE and empties so callers take their not-found
    // branch. textpose.cpp's LoadEmotionStrings depends on the string table, so the
    // native emotion rules need those strings from data - the frozen textpose golden
    // lists them.
    BOOL LoadString(UINT) { Empty(); return FALSE; }

private:
    void Assign(const char* s, size_t n) {
        char* q = (char*)malloc(n + 1);
        if (s && n) memcpy(q, s, n);
        q[n] = 0;
        free(m_pchData);
        m_pchData = q;
    }
    char* m_pchData;   // THE ONLY MEMBER - see the class comment.
};

inline CString operator+(const CString& a, const CString& b) { CString r(a); r += b; return r; }
inline CString operator+(const CString& a, const char* b)    { CString r(a); r += b; return r; }
inline CString operator+(const char* a, const CString& b)    { CString r(a); r += b; return r; }
inline bool operator==(const CString& a, const char* b)      { return a.Compare(b) == 0; }
inline bool operator!=(const CString& a, const char* b)      { return a.Compare(b) != 0; }
inline bool operator==(const CString& a, const CString& b)   { return a.Compare(b) == 0; }
inline bool operator!=(const CString& a, const CString& b)   { return a.Compare(b) != 0; }

// ---------------------------------------------------------------------------
// Array classes. GetUpperBound() returns size-1 and is -1 when empty; several
// engine loops are written `for (i = 1; i <= GetUpperBound(); i++)` and depend on
// exactly that.
// ---------------------------------------------------------------------------
template <class T>
class CArrayBase {
public:
    int GetSize() const                 { return (int)m_v.size(); }
    int GetUpperBound() const           { return (int)m_v.size() - 1; }
    int GetCount() const                { return (int)m_v.size(); }
    // The second parameter is MFC's grow-by hint; capacity policy is not
    // observable, so it is deliberately ignored rather than emulated.
    void SetSize(int n, int = -1)       { m_v.resize(n > 0 ? (size_t)n : 0); }
    void RemoveAll()                    { m_v.clear(); }
    void FreeExtra()                    { std::vector<T>(m_v).swap(m_v); }
    int Add(T x)                        { m_v.push_back(x); return (int)m_v.size() - 1; }
    T GetAt(int i) const                { return m_v[(size_t)i]; }
    void SetAt(int i, T x)              { m_v[(size_t)i] = x; }
    void SetAtGrow(int i, T x)          { if ((size_t)i >= m_v.size()) m_v.resize((size_t)i + 1, T()); m_v[(size_t)i] = x; }
    void InsertAt(int i, T x)           { m_v.insert(m_v.begin() + i, x); }
    void RemoveAt(int i, int n = 1)     { m_v.erase(m_v.begin() + i, m_v.begin() + i + n); }
    T& operator[](int i)                { return m_v[(size_t)i]; }
    const T& operator[](int i) const    { return m_v[(size_t)i]; }
    // MFC hands out the backing store; balloon.cpp passes it straight to
    // CFormatInfo as a raw pointer. Returns NULL when empty rather than a
    // one-past-the-end pointer, matching what callers null-check for.
    T* GetData()                        { return m_v.empty() ? (T*)0 : &m_v[0]; }
    const T* GetData() const            { return m_v.empty() ? (const T*)0 : &m_v[0]; }
protected:
    std::vector<T> m_v;
};

class CPtrArray : public CArrayBase<void*> {};
class CDWordArray : public CArrayBase<DWORD> {};
class CWordArray : public CArrayBase<WORD> {};
class CByteArray : public CArrayBase<BYTE> {};
class CUIntArray : public CArrayBase<UINT> {};

class CStringArray {
public:
    int GetSize() const             { return (int)m_v.size(); }
    int GetUpperBound() const       { return (int)m_v.size() - 1; }
    void SetSize(int n, int = -1)    { m_v.resize(n > 0 ? (size_t)n : 0); }
    void RemoveAll()                { m_v.clear(); }
    int Add(const CString& s)       { m_v.push_back(s); return (int)m_v.size() - 1; }
    CString GetAt(int i) const      { return m_v[(size_t)i]; }
    void SetAt(int i, const CString& s) { m_v[(size_t)i] = s; }
    void SetAtGrow(int i, const CString& s) { if ((size_t)i >= m_v.size()) m_v.resize((size_t)i + 1); m_v[(size_t)i] = s; }
    CString& operator[](int i)       { return m_v[(size_t)i]; }
private:
    std::vector<CString> m_v;
};

// CTypedPtrArray<CPtrArray, CPose*> - MFC's typed wrapper. Only the element
// accessors need the derived type; the base class supplies the rest.
template <class BASE, class TYPE>
class CTypedPtrArray : public BASE {
public:
    TYPE GetAt(int i) const          { return (TYPE)BASE::GetAt(i); }
    TYPE operator[](int i) const     { return (TYPE)BASE::GetAt(i); }
    int Add(TYPE x)                  { return BASE::Add((void*)x); }
    void SetAt(int i, TYPE x)        { BASE::SetAt(i, (void*)x); }
    void SetAtGrow(int i, TYPE x)    { BASE::SetAtGrow(i, (void*)x); }
    void InsertAt(int i, TYPE x)     { BASE::InsertAt(i, (void*)x); }
};

// ---------------------------------------------------------------------------
// Map classes. Lookup returns BOOL and writes through a reference, as MFC does.
// ---------------------------------------------------------------------------
class CMapWordToPtr {
public:
    // MFC's ctor takes a hash-table block size; capacity is not observable so the
    // hint is accepted and ignored rather than emulated.
    CMapWordToPtr(int = 10) {}
    BOOL Lookup(WORD key, void*& val) const {
        std::map<WORD, void*>::const_iterator it = m_m.find(key);
        if (it == m_m.end()) return FALSE;
        val = it->second; return TRUE;
    }
    void SetAt(WORD key, void* val) { m_m[key] = val; }
    BOOL RemoveKey(WORD key)        { return m_m.erase(key) ? TRUE : FALSE; }
    void RemoveAll()                { m_m.clear(); }
    int GetCount() const            { return (int)m_m.size(); }
    // Iteration mirrors MFC's opaque-position idiom. NOTE: MFC's hash order is
    // NOT std::map's sorted order. Any engine path whose OUTPUT depends on
    // iteration order would diverge here - flagged rather than papered over,
    // because the fix belongs at the call site, not in the container.
    void* GetStartPosition() const  { return m_m.empty() ? NULL : (void*)1; }
    void GetNextAssoc(void*& pos, WORD& key, void*& val) const {
        size_t idx = (size_t)pos - 1;
        std::map<WORD, void*>::const_iterator it = m_m.begin();
        for (size_t i = 0; i < idx && it != m_m.end(); i++) ++it;
        key = it->first; val = it->second;
        pos = (++it == m_m.end()) ? NULL : (void*)(idx + 2);
    }
private:
    std::map<WORD, void*> m_m;
};

class CMapPtrToPtr {
public:
    CMapPtrToPtr(int = 10) {}
    BOOL Lookup(void* key, void*& val) const {
        std::map<void*, void*>::const_iterator it = m_m.find(key);
        if (it == m_m.end()) return FALSE;
        val = it->second; return TRUE;
    }
    void SetAt(void* key, void* val) { m_m[key] = val; }
    BOOL RemoveKey(void* key)        { return m_m.erase(key) ? TRUE : FALSE; }
    void RemoveAll()                 { m_m.clear(); }
    int GetCount() const             { return (int)m_m.size(); }
private:
    std::map<void*, void*> m_m;
};

class CMapPtrToWord {
public:
    CMapPtrToWord(int = 10) {}
    BOOL Lookup(void* key, WORD& val) const {
        std::map<void*, WORD>::const_iterator it = m_m.find(key);
        if (it == m_m.end()) return FALSE;
        val = it->second; return TRUE;
    }
    void SetAt(void* key, WORD val) { m_m[key] = val; }
    BOOL RemoveKey(void* key)  { return m_m.erase(key) ? TRUE : FALSE; }
    void RemoveAll()           { m_m.clear(); }
    int GetCount() const       { return (int)m_m.size(); }
private:
    std::map<void*, WORD> m_m;
};

class CMapStringToString {
public:
    CMapStringToString(int = 10) {}
    BOOL Lookup(const char* key, CString& val) const {
        std::map<std::string, std::string>::const_iterator it = m_m.find(key ? key : "");
        if (it == m_m.end()) return FALSE;
        val = it->second.c_str(); return TRUE;
    }
    void SetAt(const char* key, const char* val) { m_m[key ? key : ""] = val ? val : ""; }
    BOOL RemoveKey(const char* key) { return m_m.erase(key ? key : "") ? TRUE : FALSE; }
    void RemoveAll()                { m_m.clear(); }
    int GetCount() const            { return (int)m_m.size(); }
private:
    std::map<std::string, std::string> m_m;
};

class CMapWordToOb {
public:
    CMapWordToOb(int = 10) {}
    BOOL Lookup(WORD key, CObject*& val) const {
        std::map<WORD, CObject*>::const_iterator it = m_m.find(key);
        if (it == m_m.end()) return FALSE;
        val = it->second; return TRUE;
    }
    void SetAt(WORD key, CObject* val) { m_m[key] = val; }
    BOOL RemoveKey(WORD key)  { return m_m.erase(key) ? TRUE : FALSE; }
    void RemoveAll()          { m_m.clear(); }
    int GetCount() const      { return (int)m_m.size(); }
private:
    std::map<WORD, CObject*> m_m;
};

class CMapStringToPtr {
public:
    CMapStringToPtr(int = 10) {}
    BOOL Lookup(const char* key, void*& val) const {
        std::map<std::string, void*>::const_iterator it = m_m.find(key ? key : "");
        if (it == m_m.end()) return FALSE;
        val = it->second; return TRUE;
    }
    void SetAt(const char* key, void* val) { m_m[key ? key : ""] = val; }
    BOOL RemoveKey(const char* key)  { return m_m.erase(key ? key : "") ? TRUE : FALSE; }
    void RemoveAll()                 { m_m.clear(); }
    int GetCount() const             { return (int)m_m.size(); }
    // See the ordering caveat on CMapWordToPtr: this walks in sorted order, MFC
    // walks in hash order. Only safe for callers that treat the map as a set.
    void* GetStartPosition() const   { return m_m.empty() ? NULL : (void*)1; }
    void GetNextAssoc(void*& pos, CString& key, void*& val) const {
        size_t idx = (size_t)pos - 1;
        std::map<std::string, void*>::const_iterator it = m_m.begin();
        for (size_t i = 0; i < idx && it != m_m.end(); i++) ++it;
        key = it->first.c_str(); val = it->second;
        pos = (++it == m_m.end()) ? NULL : (void*)(idx + 2);
    }
private:
    std::map<std::string, void*> m_m;
};

// CTypedPtrMap<BASE, KEY, VALUE> - MFC's typed map wrapper, same shape as
// CTypedPtrArray: the base supplies storage, the wrapper supplies the casts.
template <class BASE, class KEY, class VALUE>
class CTypedPtrMap : public BASE {
public:
    BOOL Lookup(KEY key, VALUE& val) const {
        void* p = 0;
        if (!BASE::Lookup(key, p)) return FALSE;
        val = (VALUE)p;
        return TRUE;
    }
    void SetAt(KEY key, VALUE val) { BASE::SetAt(key, (void*)val); }
    BOOL RemoveKey(KEY key) { return BASE::RemoveKey(key); }
    void GetNextAssoc(void*& pos, KEY& key, VALUE& val) const {
        void* p = 0;
        BASE::GetNextAssoc(pos, key, p);
        val = (VALUE)p;
    }
};

// ---------------------------------------------------------------------------
// CPtrList - doubly linked list with MFC's POSITION cursor.
// ---------------------------------------------------------------------------
typedef void* POSITION;

class CPtrList {
public:
    // POSITION is a 1-based index into the vector, so 0/NULL is "past the end" the
    // way MFC's null POSITION is. Everything below is written against that
    // convention; changing the encoding means changing all of it at once.
    int GetCount() const             { return (int)m_v.size(); }
    BOOL IsEmpty() const             { return m_v.empty() ? TRUE : FALSE; }
    POSITION AddTail(void* p)        { m_v.push_back(p); return (POSITION)m_v.size(); }
    POSITION AddHead(void* p)        { m_v.insert(m_v.begin(), p); return (POSITION)1; }
    void RemoveAll()                 { m_v.clear(); }

    POSITION GetHeadPosition() const { return m_v.empty() ? NULL : (POSITION)1; }
    POSITION GetTailPosition() const { return m_v.empty() ? NULL : (POSITION)m_v.size(); }

    void* GetNext(POSITION& pos) const {
        size_t idx = (size_t)pos - 1;
        void* r = m_v[idx];
        pos = (idx + 1 < m_v.size()) ? (POSITION)(idx + 2) : NULL;
        return r;
    }
    void* GetPrev(POSITION& pos) const {
        size_t idx = (size_t)pos - 1;
        void* r = m_v[idx];
        pos = (idx == 0) ? NULL : (POSITION)idx;
        return r;
    }

    void* GetAt(POSITION pos) const  { return m_v[(size_t)pos - 1]; }
    void SetAt(POSITION pos, void* p){ m_v[(size_t)pos - 1] = p; }
    // MFC's FindIndex is O(n) list walking; the index encoding makes it O(1) here.
    // Returns NULL for out of range, as MFC does.
    POSITION FindIndex(int i) const {
        return (i >= 0 && (size_t)i < m_v.size()) ? (POSITION)(i + 1) : NULL;
    }
    POSITION Find(void* p) const {
        for (size_t i = 0; i < m_v.size(); i++) if (m_v[i] == p) return (POSITION)(i + 1);
        return NULL;
    }
    void RemoveAt(POSITION pos)      { m_v.erase(m_v.begin() + ((size_t)pos - 1)); }
    POSITION InsertBefore(POSITION pos, void* p) {
        size_t idx = (size_t)pos - 1;
        m_v.insert(m_v.begin() + idx, p);
        return (POSITION)(idx + 1);
    }
    POSITION InsertAfter(POSITION pos, void* p) {
        size_t idx = (size_t)pos;
        m_v.insert(m_v.begin() + idx, p);
        return (POSITION)(idx + 1);
    }

    void* GetHead() const            { return m_v.front(); }
    void* GetTail() const            { return m_v.back(); }
    void* RemoveHead()               { void* r = m_v.front(); m_v.erase(m_v.begin()); return r; }
    void* RemoveTail()               { void* r = m_v.back(); m_v.pop_back(); return r; }
private:
    std::vector<void*> m_v;
};

// CTypedPtrList<CPtrList, T*> - the typed wrapper, same shape as CTypedPtrArray.
template <class BASE, class TYPE>
class CTypedPtrList : public BASE {
public:
    TYPE GetNext(POSITION& pos) const  { return (TYPE)BASE::GetNext(pos); }
    TYPE GetPrev(POSITION& pos) const  { return (TYPE)BASE::GetPrev(pos); }
    TYPE GetAt(POSITION pos) const     { return (TYPE)BASE::GetAt(pos); }
    void SetAt(POSITION pos, TYPE p)   { BASE::SetAt(pos, (void*)p); }
    TYPE GetHead() const               { return (TYPE)BASE::GetHead(); }
    TYPE GetTail() const               { return (TYPE)BASE::GetTail(); }
    TYPE RemoveHead()                  { return (TYPE)BASE::RemoveHead(); }
    TYPE RemoveTail()                  { return (TYPE)BASE::RemoveTail(); }
    POSITION AddTail(TYPE p)           { return BASE::AddTail((void*)p); }
    POSITION AddHead(TYPE p)           { return BASE::AddHead((void*)p); }
};
// ---------------------------------------------------------------------------
// CTime / CTimeSpan - MFC's time wrappers over time_t. Real implementations
// (script.h and the history code stamp entries), but note the native app should
// prefer these only where the engine already does; new code has better options.
// ---------------------------------------------------------------------------
#include <time.h>

class CTimeSpan {
public:
    CTimeSpan() : m_span(0) {}
    CTimeSpan(time_t s) : m_span(s) {}
    CTimeSpan(long days, int h, int m, int sec) : m_span(((days * 24 + h) * 60 + m) * 60 + sec) {}
    time_t GetTotalSeconds() const { return m_span; }
    long GetDays() const { return (long)(m_span / 86400); }
    long GetTotalMinutes() const { return (long)(m_span / 60); }
    long GetMinutes() const { return (long)((m_span / 60) % 60); }
    long GetSeconds() const { return (long)(m_span % 60); }
private:
    time_t m_span;
};

class CTime {
public:
    CTime() : m_time(0) {}
    CTime(time_t t) : m_time(t) {}
    CTime(int y, int mo, int d, int h, int mi, int s) {
        struct tm tmv;
        memset(&tmv, 0, sizeof(tmv));
        tmv.tm_year = y - 1900; tmv.tm_mon = mo - 1; tmv.tm_mday = d;
        tmv.tm_hour = h; tmv.tm_min = mi; tmv.tm_sec = s;
        tmv.tm_isdst = -1;
        m_time = mktime(&tmv);
    }
    static CTime GetCurrentTime() { return CTime(time(0)); }
    time_t GetTime() const { return m_time; }
    int GetYear() const  { struct tm t; localtime_r(&m_time, &t); return t.tm_year + 1900; }
    int GetMonth() const { struct tm t; localtime_r(&m_time, &t); return t.tm_mon + 1; }
    int GetDay() const   { struct tm t; localtime_r(&m_time, &t); return t.tm_mday; }
    int GetHour() const  { struct tm t; localtime_r(&m_time, &t); return t.tm_hour; }
    int GetMinute() const{ struct tm t; localtime_r(&m_time, &t); return t.tm_min; }
    int GetSecond() const{ struct tm t; localtime_r(&m_time, &t); return t.tm_sec; }
    CTimeSpan operator-(const CTime& o) const { return CTimeSpan(m_time - o.m_time); }
    CTime operator-(const CTimeSpan& s) const { return CTime(m_time - s.GetTotalSeconds()); }
    CTime operator+(const CTimeSpan& s) const { return CTime(m_time + s.GetTotalSeconds()); }
    bool operator<(const CTime& o) const  { return m_time < o.m_time; }
    bool operator>(const CTime& o) const  { return m_time > o.m_time; }
    bool operator==(const CTime& o) const { return m_time == o.m_time; }
    CString Format(const char* fmt) const {
        struct tm t; localtime_r(&m_time, &t);
        char buf[256];
        strftime(buf, sizeof(buf), fmt, &t);
        return CString(buf);
    }
private:
    time_t m_time;
};

// ---------------------------------------------------------------------------
// CRect / CPoint / CSize - thin wrappers over the Win32 structs, as in MFC.
// ---------------------------------------------------------------------------
class CPoint : public POINT {
public:
    CPoint() { x = y = 0; }
    CPoint(LONG X, LONG Y) { x = X; y = Y; }
    CPoint(const POINT& p) { x = p.x; y = p.y; }
};

class CSize : public SIZE {
public:
    CSize() { cx = cy = 0; }
    CSize(LONG X, LONG Y) { cx = X; cy = Y; }
    CSize(const SIZE& s) { cx = s.cx; cy = s.cy; }
};

class CRect : public RECT {
public:
    CRect() { left = top = right = bottom = 0; }
    CRect(LONG l, LONG t, LONG r, LONG b) { left = l; top = t; right = r; bottom = b; }
    CRect(const RECT& x) { left = x.left; top = x.top; right = x.right; bottom = x.bottom; }
    LONG Width() const  { return right - left; }
    LONG Height() const { return bottom - top; }
    void SetRect(LONG l, LONG t, LONG r, LONG b) { left = l; top = t; right = r; bottom = b; }
    void SetRectEmpty() { left = top = right = bottom = 0; }
    BOOL IsRectEmpty() const { return (right <= left || bottom <= top) ? TRUE : FALSE; }
};

#endif // NATIVE_SHIM_MFCSHIM_H
