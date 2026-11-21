// MUNSphere — Module2_Delegate/Delegate.h
// A delegate IS a Person (inheritance) and HAS a login account (User).
// Registration follows a small state machine:
//
//   none --apply--> pending --approve--> approved --assign--> (assigned)
//                     |  ^                                     committee +
//                  reject |                                    portfolio
//                     v  |
//                  rejected (with reason) --apply again--> pending
#pragma once

#include <string>
#include <vector>

#include "Person.h"
#include "dsa_api.h"

namespace mun {

class Delegate : public Person {
public:
    Delegate(User account, const DelegateRec& rec);

    static std::string statusKey(int status);  // none|pending|approved|rejected

    int status() const { return rec_.status; }
    std::string conferenceId() const { return rec_.conferenceId; }
    std::vector<std::string> preferences() const;
    std::string committeeId() const { return rec_.committeeId; }
    std::string portfolio() const { return rec_.portfolio; }
    std::string institution() const { return rec_.institution; }
    std::string note() const { return rec_.note; }
    bool assigned() const { return rec_.committeeId[0] != '\0'; }

    void setName(const std::string& name);         // account + BST key together
    void setInstitution(const std::string& v);

    void apply(const std::string& conferenceId, const std::vector<std::string>& prefs);
    void withdraw();
    void approve();
    void reject(const std::string& reason);
    void assign(const std::string& committeeId, const std::string& portfolio);
    void unassign();

    DelegateRec toRecord() const;

    std::string roleTitle() const override { return "Delegate"; }
    std::string summary() const override;
    Json toJson() const override;

private:
    DelegateRec rec_;
};

}  // namespace mun
