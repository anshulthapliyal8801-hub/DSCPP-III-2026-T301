// MUNSphere — Module8_Scoring/ScoringManager.h
#pragma once

#include <string>
#include <vector>

#include "API.h"
#include "AttendanceManager.h"
#include "Auth.h"
#include "Conference.h"
#include "Score.h"

namespace mun {

class ScoringManager {
public:
    ScoringManager(ConferenceManager& conf, AttendanceManager& att);

    std::vector<CriterionRec> criteria() const;
    Json criteriaJson() const;
    Json setCriteria(const Json& body);                       // Admin

    Json committeeSheets(const AuthContext& ctx, const std::string& committeeId) const;
    Json create(const AuthContext& ctx, const std::string& committeeId, const Json& body);  // EB
    Json get(const AuthContext& ctx, const std::string& id) const;
    Json save(const AuthContext& ctx, const std::string& id, const Json& body);              // EB
    Json submit(const AuthContext& ctx, const std::string& id);                              // EB
    Json review(const HttpRequest& q) const;                                                 // Admin
    Json approve(const AuthContext& ctx, const std::string& id);                             // Admin
    Json returnSheet(const AuthContext& ctx, const std::string& id, const Json& body);       // Admin
    Json publish(const AuthContext& ctx, const std::string& id);                             // Admin
    Json publishCommittee(const AuthContext& ctx, const std::string& committeeId);           // Admin
    Json mySheet(const AuthContext& ctx) const;                                              // Delegate

    Marksheet load(const std::string& id) const;

private:
    ConferenceManager& conf_;
    AttendanceManager& att_;
    void ensureDefaults();
    void store(Marksheet& m, int action, const std::string& by, const std::string& note, bool isNew);
    void requireWriter(const AuthContext& ctx, const std::string& committeeId) const;
};

void registerScoringRoutes(Router& router, ScoringManager& sm, Auth& auth);

}  // namespace mun
