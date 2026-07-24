// ojson.cpp - minimal JSON DOM implementation (see ojson.h).

#include "ojson.h"

#include <cstring>
#include <cstdlib>

namespace ojson {

// ---------------------------------------------------------------- emitting

static void EmitString(std::string &out, const std::string &s) {
    out += '"';
    for (size_t k = 0; k < s.size(); ++k) {
        unsigned char c = (unsigned char)s[k];
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20 || c >= 0x7F) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                out += buf;
            } else {
                out += (char)c;
            }
        }
    }
    out += '"';
}

static void Indent(std::string &out, int n) {
    for (int k = 0; k < n; ++k) out += "  ";
}

void Value::Emit(std::string &out, int indent) const {
    char buf[64];
    switch (type) {
    case T_NULL:   out += "null"; break;
    case T_BOOL:   out += b ? "true" : "false"; break;
    case T_INT:
        std::snprintf(buf, sizeof(buf), "%ld", i);
        out += buf;
        break;
    case T_DOUBLE:
        std::snprintf(buf, sizeof(buf), "%.17g", d);
        out += buf;
        break;
    case T_STRING: EmitString(out, s); break;
    case T_ARRAY:
        if (arr.empty()) { out += "[]"; break; }
        out += "[\n";
        for (size_t k = 0; k < arr.size(); ++k) {
            Indent(out, indent + 1);
            arr[k].Emit(out, indent + 1);
            if (k + 1 < arr.size()) out += ',';
            out += '\n';
        }
        Indent(out, indent);
        out += ']';
        break;
    case T_OBJECT:
        if (obj.empty()) { out += "{}"; break; }
        out += "{\n";
        for (size_t k = 0; k < obj.size(); ++k) {
            Indent(out, indent + 1);
            EmitString(out, obj[k].first);
            out += ": ";
            obj[k].second.Emit(out, indent + 1);
            if (k + 1 < obj.size()) out += ',';
            out += '\n';
        }
        Indent(out, indent);
        out += '}';
        break;
    }
}

// ---------------------------------------------------------------- parsing

namespace {

struct Parser {
    const char *p;
    const char *end;
    const char *begin;
    std::string *err;

    bool Fail(const char *msg) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s at byte %ld", msg, (long)(p - begin));
        *err = buf;
        return false;
    }
    void SkipWs() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }
    bool ParseValue(Value &out);

    bool ParseString(std::string &out) {
        if (p >= end || *p != '"') return Fail("expected string");
        ++p;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                ++p;
                if (p >= end) return Fail("bad escape");
                switch (*p) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    if (end - p < 5) return Fail("bad \\u escape");
                    char hex[5] = { p[1], p[2], p[3], p[4], 0 };
                    char *e = 0;
                    unsigned long cp = std::strtoul(hex, &e, 16);
                    if (e != hex + 4) return Fail("bad \\u escape");
                    if (cp > 0xFF) return Fail("\\u escape above 0xFF unsupported (corpus is byte-oriented)");
                    out += (char)(unsigned char)cp;
                    p += 4;
                    break;
                }
                default: return Fail("unknown escape");
                }
                ++p;
            } else {
                out += *p++;
            }
        }
        if (p >= end) return Fail("unterminated string");
        ++p; // closing quote
        return true;
    }

    bool ParseNumber(Value &out) {
        const char *start = p;
        if (p < end && (*p == '-' || *p == '+')) ++p;
        bool isDouble = false;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' ||
                           *p == 'E' || *p == '-' || *p == '+')) {
            if (*p == '.' || *p == 'e' || *p == 'E') isDouble = true;
            ++p;
        }
        std::string num(start, p);
        if (num.empty()) return Fail("bad number");
        if (isDouble) {
            out = Value::Dbl(std::strtod(num.c_str(), 0));
        } else {
            out = Value::Int(std::strtol(num.c_str(), 0, 10));
        }
        return true;
    }
};

bool Parser::ParseValue(Value &out) {
    SkipWs();
    if (p >= end) return Fail("unexpected end of input");
    switch (*p) {
    case '{': {
        ++p;
        out = Value::Obj();
        SkipWs();
        if (p < end && *p == '}') { ++p; return true; }
        for (;;) {
            SkipWs();
            std::string key;
            if (!ParseString(key)) return false;
            SkipWs();
            if (p >= end || *p != ':') return Fail("expected ':'");
            ++p;
            Value v;
            if (!ParseValue(v)) return false;
            out.obj.push_back(std::make_pair(key, v));
            SkipWs();
            if (p < end && *p == ',') { ++p; continue; }
            if (p < end && *p == '}') { ++p; return true; }
            return Fail("expected ',' or '}'");
        }
    }
    case '[': {
        ++p;
        out = Value::Arr();
        SkipWs();
        if (p < end && *p == ']') { ++p; return true; }
        for (;;) {
            Value v;
            if (!ParseValue(v)) return false;
            out.arr.push_back(v);
            SkipWs();
            if (p < end && *p == ',') { ++p; continue; }
            if (p < end && *p == ']') { ++p; return true; }
            return Fail("expected ',' or ']'");
        }
    }
    case '"': {
        std::string s;
        if (!ParseString(s)) return false;
        out = Value::Str(s);
        return true;
    }
    case 't':
        if (end - p >= 4 && std::memcmp(p, "true", 4) == 0) { p += 4; out = Value::Bool(true); return true; }
        return Fail("bad literal");
    case 'f':
        if (end - p >= 5 && std::memcmp(p, "false", 5) == 0) { p += 5; out = Value::Bool(false); return true; }
        return Fail("bad literal");
    case 'n':
        if (end - p >= 4 && std::memcmp(p, "null", 4) == 0) { p += 4; out = Value::Null(); return true; }
        return Fail("bad literal");
    default:
        return ParseNumber(out);
    }
}

} // namespace

bool Parse(const std::string &text, Value &out, std::string &err) {
    Parser ps;
    ps.p = text.c_str();
    ps.end = ps.p + text.size();
    ps.begin = ps.p;
    ps.err = &err;
    if (!ps.ParseValue(out)) return false;
    ps.SkipWs();
    if (ps.p != ps.end) return ps.Fail("trailing content");
    return true;
}

} // namespace ojson
