// MUNSphere — Module4_Attendance/AttendanceManager.h
#pragma once

#include <string>

#include "API.h"
#include "Auth.h"
#include "Conference.h"

namespace mun {

class AttendanceManager {
public:
    explicit AttendanceManager(ConferenceManager& conferences);

    Json sheet(const std::string& sessionId) const;
    Json mark(const AuthContext& ctx, const std::string& sessionId, const Json& body);
    Json markAll(const AuthContext& ctx, const std::string& sessionId, const Json& body);
    Json checkIn(const AuthContext& ctx, const std::string& sessionId);
    Json delegateSummary(const std::string& delegateId) const;
    Json committeeReport(const std::string& committeeId) const;

    // Present or late in this session.
    bool isSeated(const std::string& sessionId, const std::string& delegateId) const;
    // For delegate actions: placed in the session's committee AND marked
    // present or late. Throws 403/409 with a message saying which.
    void requireSeated(const AuthContext& ctx, const Session& s) const;
    // Throws 409 unless the session is active.
    static void requireActive(const Session& s);

private:
    ConferenceManager& conf_;
};

void registerAttendanceRoutes(Router& router, AttendanceManager& am, ConferenceManager& conf, Auth& auth);

}  // namespace mun
