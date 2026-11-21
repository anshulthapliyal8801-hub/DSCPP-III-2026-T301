// MUNSphere — Module11_Reports/ReportManager.h
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "API.h"
#include "Auth.h"
#include "Report.h"

namespace mun {

class ReportManager {
public:
    ReportManager(ConferenceManager& conf, AttendanceManager& att, ScoringManager& scoring, RankingManager& ranking);

    Json catalog(const AuthContext& ctx) const;
    HttpResponse run(const AuthContext& ctx, const std::string& key, const std::string& committeeId,
                     const std::string& format) const;
    Json dsaAnalytics() const;

    // Announcements
    Json announcements(const AuthContext& ctx) const;
    Json postAnnouncement(const AuthContext& ctx, const Json& body);
    void deleteAnnouncement(const std::string& id);

private:
    ConferenceManager& conf_;
    AttendanceManager& att_;
    ScoringManager& scoring_;
    RankingManager& ranking_;
    std::vector<std::unique_ptr<Report>> reports_;
    const Report& find(const std::string& key) const;
};

void registerReportRoutes(Router& router, ReportManager& rm, Auth& auth);

}  // namespace mun
