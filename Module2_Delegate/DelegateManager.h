// MUNSphere — Module2_Delegate/DelegateManager.h
#pragma once

#include <string>
#include <vector>

#include "API.h"
#include "Auth.h"
#include "Delegate.h"

namespace mun {

class ConferenceManager;

class DelegateManager {
public:
    DelegateManager(Auth& auth, ConferenceManager& conferences);

    Json registerDelegate(const Json& body);   // public self-registration
    Delegate get(const std::string& id) const;  // throws 404
    void save(const Delegate& d, bool nameChanged);

    Json me(const AuthContext& ctx) const;
    Json updateProfile(const std::string& id, const Json& body);
    Json apply(const AuthContext& ctx, const Json& body);
    Json withdraw(const AuthContext& ctx);
    Json sessionsFor(const AuthContext& ctx) const;

    Json list(const HttpRequest& q) const;
    Json approve(const std::string& id);
    Json reject(const std::string& id, const Json& body);
    void remove(const std::string& id);

    // Module 2 data-structure lab (real counts from the C code)
    Json hashState() const;
    Json bstState() const;
    Json traceSearch(const std::string& method, const std::string& key) const;

private:
    Auth& auth_;
    ConferenceManager& conferences_;
};

void registerDelegateRoutes(Router& router, DelegateManager& dm, Auth& auth);

// Delegates placed in a committee, read from the single delegate store.
std::vector<DelegateRec> delegatesInCommittee(const std::string& committeeId);
// {id, name, portfolio} for display; null if no such delegate.
Json delegateBrief(const std::string& delegateId);

}  // namespace mun
