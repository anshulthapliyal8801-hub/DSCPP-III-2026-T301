// MUNSphere — Module10_ExecutiveBoard/EBManager.h
// Admin operations on Executive Board accounts: create with a temporary
// password, change role, transfer committee, set permissions, revoke.
#pragma once

#include <memory>
#include <string>

#include "API.h"
#include "Auth.h"
#include "EBMember.h"

namespace mun {

class ConferenceManager;

class EBManager {
public:
    EBManager(Auth& auth, ConferenceManager& conferences);

    Json list(const std::string& committeeId) const;
    std::unique_ptr<EBMember> get(const std::string& id) const;  // throws 404
    Json create(const Json& body);          // {member, temporaryPassword}
    Json update(const std::string& id, const Json& body);
    Json setAccess(const std::string& id, bool active);
    Json permissionInfo() const;

private:
    Auth& auth_;
    void requireCommittee(const std::string& committeeId) const;
    void store(const EBMember& m, bool isNew);
};

void registerEBRoutes(Router& router, EBManager& eb, Auth& auth);

}  // namespace mun
