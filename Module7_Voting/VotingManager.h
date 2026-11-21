// MUNSphere — Module7_Voting/VotingManager.h
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "API.h"
#include "AttendanceManager.h"
#include "Auth.h"
#include "Conference.h"
#include "Vote.h"

namespace mun {

class VotingManager {
public:
    using CloseHook = std::function<void(const VoteRec&)>;

    VotingManager(ConferenceManager& conferences, AttendanceManager& attendance);

    Json open(const AuthContext& ctx, const std::string& sessionId, const Json& body);
    // Used by motions: procedural vote with the motion's majority rule.
    std::string openProcedural(const std::string& sessionId, const std::string& committeeId,
                               const std::string& subject, const std::string& motionId, int majority);
    bool discardIfEmpty(const std::string& voteId);  // for undo; false if ballots exist

    Json cast(const AuthContext& ctx, const std::string& voteId, const Json& body);
    Json record(const AuthContext& ctx, const std::string& voteId, const Json& body);
    Json close(const AuthContext& ctx, const std::string& voteId);
    Json details(const AuthContext& ctx, const std::string& voteId) const;
    Json history(const AuthContext& ctx, const std::string& sessionId) const;

    Json state(const std::string& sessionId, const AuthContext& ctx, bool full) const;
    void onClose(CloseHook hook) { hooks_.push_back(std::move(hook)); }

private:
    ConferenceManager& conf_;
    AttendanceManager& att_;
    std::vector<CloseHook> hooks_;
    Vote load(const std::string& voteId) const;
};

void registerVotingRoutes(Router& router, VotingManager& vm, Auth& auth);

}  // namespace mun
