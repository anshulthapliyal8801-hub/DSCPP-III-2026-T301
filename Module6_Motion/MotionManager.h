// MUNSphere — Module6_Motion/MotionManager.h
#pragma once

#include <string>

#include "API.h"
#include "AttendanceManager.h"
#include "Auth.h"
#include "Conference.h"
#include "Motion.h"
#include "SpeakerManager.h"
#include "VotingManager.h"

namespace mun {

class MotionManager {
public:
    MotionManager(ConferenceManager& conf, AttendanceManager& att, VotingManager& votes,
                  SpeakerManager& speakers);

    Json propose(const AuthContext& ctx, const std::string& sessionId, const Json& body);
    Json withdraw(const AuthContext& ctx, const std::string& motionId);
    Json ruleOutOfOrder(const AuthContext& ctx, const std::string& motionId);
    Json takeNextToVote(const AuthContext& ctx, const std::string& sessionId);
    Json undo(const AuthContext& ctx, const std::string& sessionId);
    Json state(const std::string& sessionId) const;

private:
    ConferenceManager& conf_;
    AttendanceManager& att_;
    VotingManager& votes_;
    SpeakerManager& speakers_;

    Motion load(const std::string& id) const;
    void store(const Motion& m);
    void pushAction(const Motion& m, int from, int to, const std::string& by);
    void applyResult(const VoteRec& v);  // vote closed -> motion passed/failed
};

void registerMotionRoutes(Router& router, MotionManager& mm, Auth& auth);

}  // namespace mun
