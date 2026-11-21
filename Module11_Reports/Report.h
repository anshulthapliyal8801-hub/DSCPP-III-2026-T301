// MUNSphere — Module11_Reports/Report.h
// Reports are polymorphic: each subclass knows how to build its table;
// the base class renders any table as JSON, CSV or aligned plain text.
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AttendanceManager.h"
#include "Common.h"
#include "Conference.h"
#include "RankingManager.h"
#include "ScoringManager.h"

namespace mun {

struct ReportTable {
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::pair<std::string, std::string>> summary;
    Json chart;  // optional {title, labels[], values[]}
};

struct ReportContext {
    std::string committeeId;  // "" = everything
    ConferenceManager& conf;
    AttendanceManager& att;
    ScoringManager& scoring;
    RankingManager& ranking;
};

class Report {
public:
    virtual ~Report() = default;
    virtual std::string key() const = 0;
    virtual std::string title() const = 0;
    virtual std::string description() const = 0;
    // Committee-scoped reports can also be run by that committee's EB.
    virtual bool committeeScoped() const { return false; }
    virtual bool requiresCommittee() const { return false; }
    virtual ReportTable build(const ReportContext& ctx) const = 0;

    Json toJson(const ReportTable& t, const std::string& scope) const;
    static std::string toCsv(const ReportTable& t);
    std::string toText(const ReportTable& t, const std::string& scope) const;
};

std::vector<std::unique_ptr<Report>> makeReports();
std::string fmt1(double v);   // one decimal place

}  // namespace mun
