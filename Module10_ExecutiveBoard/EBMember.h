// MUNSphere — Module10_ExecutiveBoard/EBMember.h
// EBMember is abstract; the three concrete roles override
// defaultPermissions(), roleTitle() and responsibilities().
#pragma once

#include <memory>
#include <string>

#include "Person.h"
#include "dsa_api.h"

namespace mun {

class EBMember : public Person {
public:
    // Factory: builds the right subclass for the stored role.
    static std::unique_ptr<EBMember> create(User account, const EbRec& rec);
    static int defaultPermissionsFor(int ebRole);

    const std::string& committeeId() const { return committeeId_; }
    int ebRole() const { return ebRole_; }
    int permissions() const { return permissions_; }
    bool has(int permission) const { return (permissions_ & permission) == permission; }

    void setCommittee(const std::string& committeeId);
    void setPermissions(int mask);
    EbRec toRecord() const;

    virtual int defaultPermissions() const = 0;
    virtual std::string responsibilities() const = 0;
    std::string summary() const override;
    Json toJson() const override;  // extends Person::toJson()

protected:
    EBMember(User account, const EbRec& rec);

private:
    std::string committeeId_;
    int ebRole_;
    int permissions_;
};

class Chairperson final : public EBMember {
public:
    Chairperson(User a, const EbRec& r) : EBMember(std::move(a), r) {}
    std::string roleTitle() const override { return "Chairperson"; }
    int defaultPermissions() const override;
    std::string responsibilities() const override;
};

class Moderator final : public EBMember {
public:
    Moderator(User a, const EbRec& r) : EBMember(std::move(a), r) {}
    std::string roleTitle() const override { return "Moderator"; }
    int defaultPermissions() const override;
    std::string responsibilities() const override;
};

class Scribe final : public EBMember {
public:
    Scribe(User a, const EbRec& r) : EBMember(std::move(a), r) {}
    std::string roleTitle() const override { return "Scribe"; }
    int defaultPermissions() const override;
    std::string responsibilities() const override;
};

}  // namespace mun
