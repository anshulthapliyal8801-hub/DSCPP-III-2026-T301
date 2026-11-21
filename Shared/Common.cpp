// MUNSphere — Shared/Common.cpp
#include "Common.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace mun {

// ================================================================ JSON

bool Json::asBool() const {
    if (type_ != Type::Bool) throw JsonError("value is not a boolean");
    return bool_;
}
double Json::asNumber() const {
    if (type_ != Type::Number) throw JsonError("value is not a number");
    return num_;
}
const std::string& Json::asString() const {
    if (type_ != Type::String) throw JsonError("value is not a string");
    return str_;
}
const Json::Array& Json::asArray() const {
    if (type_ != Type::Array) throw JsonError("value is not an array");
    return arr_;
}
const Json::Object& Json::asObject() const {
    if (type_ != Type::Object) throw JsonError("value is not an object");
    return obj_;
}

bool Json::has(const std::string& key) const {
    if (type_ != Type::Object) return false;
    for (const auto& kv : obj_)
        if (kv.first == key) return true;
    return false;
}

const Json& Json::operator[](const std::string& key) const {
    static const Json kNull;
    if (type_ != Type::Object) return kNull;
    for (const auto& kv : obj_)
        if (kv.first == key) return kv.second;
    return kNull;
}

Json& Json::set(const std::string& key, Json value) {
    if (type_ == Type::Null) type_ = Type::Object;
    if (type_ != Type::Object) throw JsonError("set() on a non-object");
    for (auto& kv : obj_) {
        if (kv.first == key) { kv.second = std::move(value); return *this; }
    }
    obj_.emplace_back(key, std::move(value));
    return *this;
}

void Json::push(Json value) {
    if (type_ == Type::Null) type_ = Type::Array;
    if (type_ != Type::Array) throw JsonError("push() on a non-array");
    arr_.push_back(std::move(value));
}

std::size_t Json::size() const {
    if (type_ == Type::Array) return arr_.size();
    if (type_ == Type::Object) return obj_.size();
    return 0;
}

static void dumpString(const std::string& s, std::string& out) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);  // UTF-8 passes through
            }
        }
    }
    out += '"';
}

void Json::dumpTo(std::string& out) const {
    switch (type_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += bool_ ? "true" : "false"; break;
    case Type::Number: {
        if (!std::isfinite(num_)) { out += "null"; break; }
        char buf[32];
        if (std::floor(num_) == num_ && std::fabs(num_) < 1e15)
            std::snprintf(buf, sizeof buf, "%.0f", num_);
        else
            std::snprintf(buf, sizeof buf, "%.17g", num_);
        out += buf;
        break;
    }
    case Type::String: dumpString(str_, out); break;
    case Type::Array: {
        out += '[';
        for (std::size_t i = 0; i < arr_.size(); ++i) {
            if (i) out += ',';
            arr_[i].dumpTo(out);
        }
        out += ']';
        break;
    }
    case Type::Object: {
        out += '{';
        for (std::size_t i = 0; i < obj_.size(); ++i) {
            if (i) out += ',';
            dumpString(obj_[i].first, out);
            out += ':';
            obj_[i].second.dumpTo(out);
        }
        out += '}';
        break;
    }
    }
}

std::string Json::dump() const {
    std::string out;
    dumpTo(out);
    return out;
}

// ---- Recursive-descent parser ---------------------------------------
namespace {

class Parser {
public:
    explicit Parser(const std::string& t) : t_(t) {}

    Json parseDocument() {
        Json v = parseValue(0);
        skipWs();
        if (i_ != t_.size()) fail("unexpected text after JSON value");
        return v;
    }

private:
    const std::string& t_;
    std::size_t i_ = 0;
    static constexpr int kMaxDepth = 64;  // guards against stack exhaustion

    [[noreturn]] void fail(const std::string& msg) const {
        throw JsonError("Invalid JSON at position " + std::to_string(i_) +
                        ": " + msg);
    }
    void skipWs() {
        while (i_ < t_.size() &&
               (t_[i_] == ' ' || t_[i_] == '\t' || t_[i_] == '\n' || t_[i_] == '\r'))
            ++i_;
    }
    bool consume(const char* word) {
        std::size_t n = std::char_traits<char>::length(word);
        if (t_.compare(i_, n, word) == 0) { i_ += n; return true; }
        return false;
    }

    Json parseValue(int depth) {
        if (depth > kMaxDepth) fail("nesting too deep");
        skipWs();
        if (i_ >= t_.size()) fail("unexpected end of input");
        char c = t_[i_];
        if (c == '{') return parseObject(depth);
        if (c == '[') return parseArray(depth);
        if (c == '"') return Json(parseString());
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        if (consume("true")) return Json(true);
        if (consume("false")) return Json(false);
        if (consume("null")) return Json();
        fail(std::string("unexpected character '") + c + "'");
    }

    Json parseObject(int depth) {
        Json obj = Json::object();
        ++i_;  // '{'
        skipWs();
        if (i_ < t_.size() && t_[i_] == '}') { ++i_; return obj; }
        for (;;) {
            skipWs();
            if (i_ >= t_.size() || t_[i_] != '"') fail("expected a string key");
            std::string key = parseString();
            skipWs();
            if (i_ >= t_.size() || t_[i_] != ':') fail("expected ':'");
            ++i_;
            obj.set(key, parseValue(depth + 1));
            skipWs();
            if (i_ < t_.size() && t_[i_] == ',') { ++i_; continue; }
            if (i_ < t_.size() && t_[i_] == '}') { ++i_; return obj; }
            fail("expected ',' or '}'");
        }
    }

    Json parseArray(int depth) {
        Json arr = Json::array();
        ++i_;  // '['
        skipWs();
        if (i_ < t_.size() && t_[i_] == ']') { ++i_; return arr; }
        for (;;) {
            arr.push(parseValue(depth + 1));
            skipWs();
            if (i_ < t_.size() && t_[i_] == ',') { ++i_; continue; }
            if (i_ < t_.size() && t_[i_] == ']') { ++i_; return arr; }
            fail("expected ',' or ']'");
        }
    }

    unsigned parseHex4() {
        if (i_ + 4 > t_.size()) fail("truncated \\u escape");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            char h = t_[i_++];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= unsigned(h - '0');
            else if (h >= 'a' && h <= 'f') v |= unsigned(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') v |= unsigned(h - 'A' + 10);
            else fail("bad hex digit in \\u escape");
        }
        return v;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += char(cp);
        } else if (cp < 0x800) {
            out += char(0xC0 | (cp >> 6));
            out += char(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += char(0xE0 | (cp >> 12));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | (cp >> 18));
            out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
    }

    std::string parseString() {
        std::string out;
        ++i_;  // opening quote
        while (i_ < t_.size()) {
            char c = t_[i_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (i_ >= t_.size()) break;
            char e = t_[i_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned cp = parseHex4();
                if (cp >= 0xD800 && cp <= 0xDBFF) {  // surrogate pair
                    if (!(i_ + 1 < t_.size() && t_[i_] == '\\' && t_[i_ + 1] == 'u'))
                        fail("unpaired surrogate");
                    i_ += 2;
                    unsigned lo = parseHex4();
                    if (lo < 0xDC00 || lo > 0xDFFF) fail("invalid low surrogate");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    fail("unpaired surrogate");
                }
                appendUtf8(out, cp);
                break;
            }
            default: fail("unknown escape");
            }
        }
        fail("unterminated string");
    }

    Json parseNumber() {
        std::size_t start = i_;
        if (t_[i_] == '-') ++i_;
        if (i_ >= t_.size() || !std::isdigit(static_cast<unsigned char>(t_[i_])))
            fail("bad number");
        if (t_[i_] == '0') ++i_;
        else while (i_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[i_]))) ++i_;
        if (i_ < t_.size() && t_[i_] == '.') {
            ++i_;
            if (i_ >= t_.size() || !std::isdigit(static_cast<unsigned char>(t_[i_])))
                fail("bad fraction");
            while (i_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[i_]))) ++i_;
        }
        if (i_ < t_.size() && (t_[i_] == 'e' || t_[i_] == 'E')) {
            ++i_;
            if (i_ < t_.size() && (t_[i_] == '+' || t_[i_] == '-')) ++i_;
            if (i_ >= t_.size() || !std::isdigit(static_cast<unsigned char>(t_[i_])))
                fail("bad exponent");
            while (i_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[i_]))) ++i_;
        }
        try {
            return Json(std::stod(t_.substr(start, i_ - start)));
        } catch (const std::exception&) {
            fail("number out of range");
        }
    }
};

}  // namespace

Json Json::parse(const std::string& text) {
    Parser p(text);
    return p.parseDocument();
}

// ========================================================== Validation
namespace validate {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static bool allDigits(const std::string& s, std::size_t from, std::size_t n) {
    for (std::size_t k = from; k < from + n; ++k)
        if (!std::isdigit(static_cast<unsigned char>(s[k]))) return false;
    return true;
}

bool isDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    if (!allDigits(s, 0, 4) || !allDigits(s, 5, 2) || !allDigits(s, 8, 2)) return false;
    int y = std::stoi(s.substr(0, 4));
    int m = std::stoi(s.substr(5, 2));
    int d = std::stoi(s.substr(8, 2));
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1) return false;
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    int max = days[m - 1] + ((m == 2 && leap) ? 1 : 0);
    return d <= max;
}

bool isTime(const std::string& s) {
    if (s.size() != 5 || s[2] != ':') return false;
    if (!allDigits(s, 0, 2) || !allDigits(s, 3, 2)) return false;
    int h = std::stoi(s.substr(0, 2));
    int m = std::stoi(s.substr(3, 2));
    return h <= 23 && m <= 59;
}

bool isId(const std::string& s) {
    if (s.size() < 2 || s.size() > 15) return false;
    for (char c : s) {
        bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

}  // namespace validate

// ================================================== Request field helpers

std::string requireString(const Json& body, const std::string& key,
                          std::size_t maxLen) {
    const Json& v = body[key];
    if (v.isNull())
        throw ApiError(400, "VALIDATION", "Field '" + key + "' is required.");
    if (!v.isString())
        throw ApiError(400, "VALIDATION", "Field '" + key + "' must be text.");
    std::string s = validate::trim(v.asString());
    if (s.empty())
        throw ApiError(400, "VALIDATION", "Field '" + key + "' can't be empty.");
    if (s.size() > maxLen)
        throw ApiError(400, "VALIDATION", "Field '" + key + "' is too long (max " +
                                              std::to_string(maxLen) + " characters).");
    return s;
}

std::string optionalString(const Json& body, const std::string& key,
                           std::size_t maxLen, const std::string& fallback) {
    const Json& v = body[key];
    if (v.isNull()) return fallback;
    if (!v.isString())
        throw ApiError(400, "VALIDATION", "Field '" + key + "' must be text.");
    std::string s = validate::trim(v.asString());
    if (s.size() > maxLen)
        throw ApiError(400, "VALIDATION", "Field '" + key + "' is too long (max " +
                                              std::to_string(maxLen) + " characters).");
    return s;
}

int requireInt(const Json& body, const std::string& key, int min, int max) {
    const Json& v = body[key];
    if (v.isNull())
        throw ApiError(400, "VALIDATION", "Field '" + key + "' is required.");
    if (!v.isNumber() || std::floor(v.asNumber()) != v.asNumber())
        throw ApiError(400, "VALIDATION", "Field '" + key + "' must be a whole number.");
    double d = v.asNumber();
    if (d < min || d > max)
        throw ApiError(400, "VALIDATION", "Field '" + key + "' must be between " +
                                              std::to_string(min) + " and " +
                                              std::to_string(max) + ".");
    return static_cast<int>(d);
}

std::string nowTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    // localtime is not thread-safe, but the server is single-threaded,
    // and it avoids depending on MinGW's optional secure-API functions.
    tmv = *std::localtime(&t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

long long nowEpoch() { return static_cast<long long>(std::time(nullptr)); }

}  // namespace mun
