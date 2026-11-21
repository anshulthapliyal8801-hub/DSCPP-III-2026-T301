/*
 * MUNSphere — Module10_ExecutiveBoard/Person.h
 *
 *                   Person  (abstract)
 *                  /       \
 *           Delegate        EBMember  (abstract)
 *                          /    |     \
 *                Chairperson Moderator Scribe
 *
 * A Person HAS a User (the login account): composition. Delegate and
 * EBMember ARE Persons: inheritance. roleTitle() and summary() are pure
 * virtual, so each subclass must define them (polymorphism).
 */
#pragma once

#include <string>

#include "Common.h"
#include "User.h"

namespace mun {

class Person : public Entity {
public:
    Person(User account, const std::string& email);

    const User& account() const { return account_; }
    User& account() { return account_; }
    const std::string& name() const { return account_.name(); }
    const std::string& email() const { return email_; }
    bool active() const { return account_.active(); }

    void setEmail(const std::string& email);
    static std::string normalizeEmail(const std::string& email);  // throws 400

    virtual std::string roleTitle() const = 0;
    virtual std::string summary() const = 0;
    std::string entityName() const override { return roleTitle(); }

    // Base fields shared by every person; subclasses extend it.
    Json toJson() const override;

protected:
    User account_;
    std::string email_;
};

}  // namespace mun
