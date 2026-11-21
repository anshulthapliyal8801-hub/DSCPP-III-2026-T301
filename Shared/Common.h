// MUNSphere — Shared/Common.h
// Building blocks used by every module: a JSON value type, the API error
// type, the abstract Entity base class and input-validation helpers.
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mun {

// ---------------------------------------------------------------- JSON
class JsonError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A small JSON value. Objects keep insertion order (stored as a vector of
// key/value pairs), which keeps API output readable and matches field
// order in the source code. Lookup is linear, which is fine for the small
// objects an API exchanges.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    Json() : type_(Type::Null) {}
    Json(std::nullptr_t) : type_(Type::Null) {}
    Json(bool b) : type_(Type::Bool), bool_(b) {}
    template <typename T,
              typename = std::enable_if_t<std::is_arithmetic_v<T> &&
                                          !std::is_same_v<T, bool>>>
    Json(T n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Json(const char* s) : type_(Type::String), str_(s ? s : "") {}
    Json(std::string s) : type_(Type::String), str_(std::move(s)) {}

    static Json array() { Json j; j.type_ = Type::Array; return j; }
    static Json object() { Json j; j.type_ = Type::Object; return j; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool() const;
    double asNumber() const;
    const std::string& asString() const;
    const Array& asArray() const;
    const Object& asObject() const;

    bool has(const std::string& key) const;
    // Missing keys (or non-objects) return a shared null value.
    const Json& operator[](const std::string& key) const;
    // Inserts or replaces a key; returns *this so calls can be chained.
    Json& set(const std::string& key, Json value);
    void push(Json value);
    std::size_t size() const;

    std::string dump() const;
    static Json parse(const std::string& text);  // throws JsonError

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    Array arr_;
    Object obj_;

    void dumpTo(std::string& out) const;
};

// ---------------------------------------------------------- API errors
// Thrown by business logic; the router turns it into an HTTP response
// such as 404 {"ok":false,"error":{"code":"NOT_FOUND","message":...}}.
class ApiError : public std::runtime_error {
public:
    ApiError(int status, std::string code, const std::string& message)
        : std::runtime_error(message), status_(status), code_(std::move(code)) {}
    int status() const { return status_; }
    const std::string& code() const { return code_; }

private:
    int status_;
    std::string code_;
};

// ------------------------------------------------------- Entity (OOP)
// Abstract base for every domain object (Conference, Committee, Session,
// and later Delegate, Motion, ...). Pure virtual functions make every
// subclass define its own JSON form and name: runtime polymorphism.
class Entity {
public:
    virtual ~Entity() = default;
    const std::string& id() const { return id_; }
    virtual Json toJson() const = 0;
    virtual std::string entityName() const = 0;

protected:
    explicit Entity(std::string id) : id_(std::move(id)) {}
    std::string id_;
};

// ------------------------------------------------------- Validation
namespace validate {
std::string trim(const std::string& s);
bool isDate(const std::string& s);   // YYYY-MM-DD, a real calendar date
bool isTime(const std::string& s);   // HH:MM, 00:00 to 23:59
bool isId(const std::string& s);     // 2-15 chars: A-Z 0-9 _ -
}  // namespace validate

// Reading fields from a request body. All throw ApiError(400, ...)
// with a message that names the field.
std::string requireString(const Json& body, const std::string& key,
                          std::size_t maxLen);
std::string optionalString(const Json& body, const std::string& key,
                           std::size_t maxLen, const std::string& fallback);
int requireInt(const Json& body, const std::string& key, int min, int max);

// Current local time as "YYYY-MM-DD HH:MM:SS".
std::string nowTimestamp();
// Seconds since 1970 (used for speaking timers shared with the browser).
long long nowEpoch();

}  // namespace mun
