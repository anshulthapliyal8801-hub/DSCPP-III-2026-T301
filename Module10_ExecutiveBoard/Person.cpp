// MUNSphere — Module10_ExecutiveBoard/Person.cpp
#include "Person.h"

#include <cctype>

#include "dsa_api.h"

namespace mun {

Person::Person(User account, const std::string& email)
    : Entity(account.id()), account_(std::move(account)) {
    setEmail(email);
}

std::string Person::normalizeEmail(const std::string& raw) {
    std::string e = validate::trim(raw);
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::size_t at = e.find('@');
    bool ok = !e.empty() && e.size() <= DSA_NAME_LEN - 1 && at != std::string::npos && at > 0 &&
              e.find('@', at + 1) == std::string::npos && e.find('.', at) != std::string::npos &&
              e.back() != '.' && e.find(' ') == std::string::npos;
    if (!ok) throw ApiError(400, "VALIDATION", "Enter a valid email address, like name@example.com.");
    return e;
}

void Person::setEmail(const std::string& email) { email_ = normalizeEmail(email); }

Json Person::toJson() const {
    Json j = Json::object();
    j.set("id", id_)
        .set("name", name())
        .set("email", email_)
        .set("roleTitle", roleTitle())
        .set("summary", summary())
        .set("active", active())
        .set("mustChangePassword", account_.mustChangePassword());
    return j;
}

}  // namespace mun
