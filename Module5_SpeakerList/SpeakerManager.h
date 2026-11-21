// MUNSphere — Module5_SpeakerList/SpeakerManager.h
// The live floor: General Speakers List with timer, speaking requests,
// and Points of Information.
#pragma once

#include <string>

#include "API.h"
#include "AttendanceManager.h"
#include "Auth.h"
#include "Conference.h"

namespace mun {

class SpeakerManager {
public:
    SpeakerManager(ConferenceManager& conferences, AttendanceManager& attendance);

    // EB (speakers permission)
    void addSpeaker(const AuthContext& ctx, const std::string& sessionId, const Json& body);
    void removeSpeaker(const AuthContext& ctx, const std::string& sessionId, const std::string& delegateId);
    void nextSpeaker(const AuthContext& ctx, const std::string& sessionId);
    void finishSpeech(const AuthContext& ctx, const std::string& sessionId);
    void setTime(const AuthContext& ctx, const std::string& sessionId, const Json& body);
    void decideRequest(const AuthContext& ctx, const std::string& sessionId, bool approve);
    // Delegates
    void requestToSpeak(const AuthContext& ctx, const std::string& sessionId);
    void withdrawRequest(const AuthContext& ctx, const std::string& sessionId);
    // POIs
    Json raisePoi(const AuthContext& ctx, const std::string& sessionId, const Json& body);
    Json decidePoi(const AuthContext& ctx, const std::string& poiId, bool accept, const Json& body);
    Json withdrawPoi(const AuthContext& ctx, const std::string& poiId);
    Json myPois(const AuthContext& ctx) const;

    // Live view piece. `full` = EB/Admin view (requests and all pending POIs).
    Json state(const std::string& sessionId, const AuthContext& ctx, bool full) const;
    // Applied when a "set speaking time" motion passes.
    void applySpeakingTime(const std::string& sessionId, int seconds);

private:
    ConferenceManager& conf_;
    AttendanceManager& att_;
    Session activeFor(const AuthContext& ctx, const std::string& sessionId, int permission) const;
};

void registerSpeakerRoutes(Router& router, SpeakerManager& sm, Auth& auth);

}  // namespace mun
