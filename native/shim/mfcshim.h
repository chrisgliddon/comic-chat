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
inline void AfxThrowNotSupportedException() { throw new CNotSupportedException(); }

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
class CObject {
public:
    virtual ~CObject() {}
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
// CString. std::string inside, MFC's interface outside.
// ---------------------------------------------------------------------------
class CString {
public:
    CString() {}
    CString(const char* s) : m_s(s ? s : "") {}
    CString(const char* s, int n) : m_s(s ? std::string(s, (size_t)n) : std::string()) {}
    CString(const CString& o) : m_s(o.m_s) {}
    CString(char c, int n) : m_s((size_t)n, c) {}

    CString& operator=(const CString& o) { m_s = o.m_s; return *this; }
    CString& operator=(const char* s)    { m_s = s ? s : ""; return *this; }
    CString& operator=(char c)           { m_s.assign(1, c); return *this; }

    // MFC's implicit conversion to LPCTSTR is used pervasively - the engine
    // passes a CString straight to printf("%s") and to strcmp.
    operator const char*() const { return m_s.c_str(); }

    int GetLength() const           { return (int)m_s.size(); }
    BOOL IsEmpty() const            { return m_s.empty() ? TRUE : FALSE; }
    void Empty()                    { m_s.clear(); }
    // Reads the terminating NUL at GetLength() the way MFC does; the engine
    // relies on that when scanning to end of string.
    char GetAt(int i) const         { return (i >= 0 && (size_t)i < m_s.size()) ? m_s[(size_t)i] : '\0'; }
    char operator[](int i) const    { return GetAt(i); }
    void SetAt(int i, char c)       { if (i >= 0 && (size_t)i < m_s.size()) m_s[(size_t)i] = c; }

    CString& operator+=(const char* s)   { if (s) m_s += s; return *this; }
    CString& operator+=(const CString& o){ m_s += o.m_s; return *this; }
    CString& operator+=(char c)          { m_s += c; return *this; }

    int Compare(const char* s) const     { return strcmp(m_s.c_str(), s ? s : ""); }
    int CompareNoCase(const char* s) const { return strcasecmp(m_s.c_str(), s ? s : ""); }

    int Find(char c) const               { size_t p = m_s.find(c); return p == std::string::npos ? -1 : (int)p; }
    int Find(const char* s) const        { size_t p = m_s.find(s ? s : ""); return p == std::string::npos ? -1 : (int)p; }
    int ReverseFind(char c) const        { size_t p = m_s.rfind(c); return p == std::string::npos ? -1 : (int)p; }

    CString Mid(int start) const         { return (start < 0 || (size_t)start > m_s.size()) ? CString() : CString(m_s.substr((size_t)start).c_str()); }
    CString Mid(int start, int n) const {
        if (start < 0 || (size_t)start > m_s.size() || n <= 0) return CString();
        return CString(m_s.substr((size_t)start, (size_t)n).c_str());
    }
    CString Left(int n) const            { return Mid(0, n); }
    CString Right(int n) const           { int L = GetLength(); return n >= L ? *this : Mid(L - n); }

    void MakeUpper() { for (size_t i = 0; i < m_s.size(); i++) m_s[i] = (char)toupper((unsigned char)m_s[i]); }
    void MakeLower() { for (size_t i = 0; i < m_s.size(); i++) m_s[i] = (char)tolower((unsigned char)m_s[i]); }
    void TrimRight()  { while (!m_s.empty() && isspace((unsigned char)m_s[m_s.size()-1])) m_s.erase(m_s.size()-1); }
    void TrimLeft()   { size_t i = 0; while (i < m_s.size() && isspace((unsigned char)m_s[i])) i++; m_s.erase(0, i); }

    void Format(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        char buf[2048];
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n >= 0 && (size_t)n < sizeof(buf)) { m_s.assign(buf, (size_t)n); return; }
        // Long results are rare here but must not truncate silently.
        va_list ap2; va_start(ap2, fmt);
        std::vector<char> big((size_t)(n > 0 ? n + 1 : 8192));
        vsnprintf(&big[0], big.size(), fmt, ap2);
        va_end(ap2);
        m_s.assign(&big[0]);
    }

    // GetBuffer/ReleaseBuffer: the engine writes through these. The buffer must
    // stay valid and writable until ReleaseBuffer, so the string is grown first.
    char* GetBuffer(int minLen) {
        if (minLen > 0 && (size_t)minLen > m_s.size()) m_s.resize((size_t)minLen, '\0');
        m_buf = m_s;
        m_buf.resize(m_buf.size() + 1, '\0');
        return &m_buf[0];
    }
    void ReleaseBuffer(int newLen = -1) {
        if (newLen < 0) m_s.assign(m_buf.c_str());
        else m_s.assign(m_buf.c_str(), (size_t)newLen);
        m_buf.clear();
    }

    const std::string& Str() const { return m_s; }

private:
    std::string m_s;
    std::string m_buf;
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

// ---------------------------------------------------------------------------
// CPtrList - doubly linked list with MFC's POSITION cursor.
// ---------------------------------------------------------------------------
typedef void* POSITION;

class CPtrList {
public:
    int GetCount() const            { return (int)m_v.size(); }
    BOOL IsEmpty() const            { return m_v.empty() ? TRUE : FALSE; }
    POSITION AddTail(void* p)       { m_v.push_back(p); return (POSITION)m_v.size(); }
    POSITION AddHead(void* p)       { m_v.insert(m_v.begin(), p); return (POSITION)1; }
    void RemoveAll()                { m_v.clear(); }
    POSITION GetHeadPosition() const { return m_v.empty() ? NULL : (POSITION)1; }
    void* GetNext(POSITION& pos) const {
        size_t idx = (size_t)pos - 1;
        void* r = m_v[idx];
        pos = (idx + 1 < m_v.size()) ? (POSITION)(idx + 2) : NULL;
        return r;
    }
    void* GetHead() const           { return m_v.front(); }
    void* GetTail() const           { return m_v.back(); }
    void* RemoveHead()              { void* r = m_v.front(); m_v.erase(m_v.begin()); return r; }
private:
    std::vector<void*> m_v;
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
