// ojson.h - minimal JSON DOM (parse + deterministic emit) for the oracle
// harness. No third-party dependencies; C++14; byte-oriented (strings are
// raw byte sequences in the app's MBCS code page, with \uXXNN escapes
// interpreted as Latin-1/CP-1252 bytes 0x00-0xFF).
//
// Determinism contract for Emit():
//   - object keys emitted in insertion order
//   - 2-space indentation, "\n" line endings, no trailing whitespace
//   - integers emitted as %ld, doubles as %.17g
// Two Emit() calls on equal trees produce byte-identical output.

#ifndef ORACLE_OJSON_H
#define ORACLE_OJSON_H

#include <string>
#include <vector>
#include <utility>
#include <cstdio>

namespace ojson {

enum Type { T_NULL, T_BOOL, T_INT, T_DOUBLE, T_STRING, T_ARRAY, T_OBJECT };

class Value {
public:
    Type type;
    bool b;
    long i;
    double d;
    std::string s;                                   // raw bytes
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value> > obj; // insertion order

    Value() : type(T_NULL), b(false), i(0), d(0.0) {}

    static Value Null() { return Value(); }
    static Value Bool(bool v) { Value x; x.type = T_BOOL; x.b = v; return x; }
    static Value Int(long v) { Value x; x.type = T_INT; x.i = v; return x; }
    static Value Dbl(double v) { Value x; x.type = T_DOUBLE; x.d = v; return x; }
    static Value Str(const std::string &v) { Value x; x.type = T_STRING; x.s = v; return x; }
    static Value Arr() { Value x; x.type = T_ARRAY; return x; }
    static Value Obj() { Value x; x.type = T_OBJECT; return x; }

    // -- object helpers ----------------------------------------------------
    Value &Set(const std::string &key, const Value &v) {
        for (size_t k = 0; k < obj.size(); ++k)
            if (obj[k].first == key) { obj[k].second = v; return obj[k].second; }
        obj.push_back(std::make_pair(key, v));
        return obj.back().second;
    }
    const Value *Find(const std::string &key) const {
        for (size_t k = 0; k < obj.size(); ++k)
            if (obj[k].first == key) return &obj[k].second;
        return 0;
    }
    Value *Find(const std::string &key) {
        for (size_t k = 0; k < obj.size(); ++k)
            if (obj[k].first == key) return &obj[k].second;
        return 0;
    }
    // Typed getters with defaults (missing key or wrong type -> default).
    long GetInt(const std::string &key, long def) const {
        const Value *v = Find(key);
        if (!v) return def;
        if (v->type == T_INT) return v->i;
        if (v->type == T_DOUBLE) return (long)v->d;
        return def;
    }
    bool GetBool(const std::string &key, bool def) const {
        const Value *v = Find(key);
        return (v && v->type == T_BOOL) ? v->b : def;
    }
    std::string GetStr(const std::string &key, const std::string &def) const {
        const Value *v = Find(key);
        return (v && v->type == T_STRING) ? v->s : def;
    }

    void Push(const Value &v) { arr.push_back(v); }

    // -- emit --------------------------------------------------------------
    void Emit(std::string &out, int indent = 0) const;
    std::string EmitToString() const { std::string o; Emit(o, 0); o += "\n"; return o; }
};

// Parse a complete JSON document from bytes. Returns true on success.
// On failure, err gets a message with byte offset.
bool Parse(const std::string &text, Value &out, std::string &err);

} // namespace ojson

#endif // ORACLE_OJSON_H
