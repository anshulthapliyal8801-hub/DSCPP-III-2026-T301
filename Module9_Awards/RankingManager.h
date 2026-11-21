// MUNSphere — Module9_Awards/RankingManager.h
#pragma once

#include <string>
#include <vector>

#include "API.h"
#include "AttendanceManager.h"
#include "Auth.h"
#include "Award.h"
#include "Conference.h"
#include "ScoringManager.h"

namespace mun {

class RankingManager {
public:
    RankingManager(ConferenceManager& conf, AttendanceManager& att, ScoringManager& scoring);

    Json config() const;
    Json setConfig(const Json& body);   // Admin

    // scope: "published" | "approved" (approved + published) | "all" (provisional)
    Json committeeRanking(const AuthContext& ctx, const std::string& committeeId,
                          const std::string& algorithm, const std::string& scope) const;
    Json overallRanking(const std::string& algorithm, const std::string& scope) const;
    Json compare(const std::string& committeeId, const std::string& scope) const;
    Json benchmark(int n, const std::string& order) const;
    Json myResults(const AuthContext& ctx) const;

private:
    ConferenceManager& conf_;
    AttendanceManager& att_;
    ScoringManager& scoring_;

    void ensureDefaults();
    std::vector<RankEntry> entries(const std::string& committeeId, const std::string& scope) const;
    std::vector<AwardRule> rules() const;
    Json rankingJson(const Ranking& r, bool withAwards) const;
};

void registerRankingRoutes(Router& router, RankingManager& rm, Auth& auth);

}  // namespace mun
