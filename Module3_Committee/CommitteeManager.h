// MUNSphere — Module3_Committee/CommitteeManager.h
// Delegate placement: assignment with capacity and portfolio checks, and
// committee rosters built from the single delegate store.
#pragma once

#include <string>

#include "API.h"
#include "Auth.h"

namespace mun {

class ConferenceManager;
class DelegateManager;

class CommitteeManager {
public:
    CommitteeManager(DelegateManager& delegates, ConferenceManager& conferences);

    int assignedCount(const std::string& committeeId) const;
    Json roster(const std::string& committeeId, const std::string& sortKey) const;
    Json summaries(const std::string& conferenceId) const;
    Json assign(const std::string& delegateId, const Json& body);
    Json unassign(const std::string& delegateId);

private:
    DelegateManager& delegates_;
    ConferenceManager& conferences_;
};

void registerCommitteeRoutes(Router& router, CommitteeManager& cm, Auth& auth);

}  // namespace mun
