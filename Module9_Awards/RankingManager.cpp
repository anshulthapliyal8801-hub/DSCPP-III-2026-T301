// MUNSphere — Module9_Awards/RankingManager.cpp
#include "RankingManager.h"

#include <cmath>
#include <cstdio>
#include <random>

#include "Database.h"
#include "DelegateManager.h"
#include "RoleManager.h"

namespace mun {

namespace {
void save() { Database::instance().save(Store::Awards); }

std::vector<int> statusesFor(const std::string& scope) {
    if (scope == "published") return {SHEET_PUBLISHED};
    if (scope == "approved" || scope.empty()) return {SHEET_APPROVED, SHEET_PUBLISHED};
    if (scope == "all") return {SHEET_DRAFT, SHEET_SUBMITTED, SHEET_RETURNED, SHEET_APPROVED, SHEET_PUBLISHED};
    throw ApiError(400, "VALIDATION", "Scope must be published, approved or all.");
}
}  // namespace

RankingManager::RankingManager(ConferenceManager& conf, AttendanceManager& att, ScoringManager& scoring)
    : conf_(conf), att_(att), scoring_(scoring) {
    ensureDefaults();
}

void RankingManager::ensureDefaults() {
    if (awards_count() > 0) return;
    struct D { const char* name; int positions; int min; };
    const D defs[] = {{"Best Delegate", 1, 70}, {"High Commendation", 1, 65},
                      {"Special Mention", 1, 60}, {"Honourable Mention", 2, 55}};
    AwardRule r[4];
    for (int i = 0; i < 4; ++i) {
        r[i] = AwardRule{};
        pers_copy_field(r[i].name, defs[i].name, AWARD_NAME_LEN);
        r[i].positions = defs[i].positions;
        r[i].minPercent = defs[i].min;
    }
    awards_set(r, 4, "speaking");
    if (Database::instance().isOpen()) save();
}

std::vector<AwardRule> RankingManager::rules() const {
    std::vector<AwardRule> v(MAX_AWARDS);
    v.resize(static_cast<std::size_t>(awards_get(v.data(), MAX_AWARDS)));
    return v;
}

Json RankingManager::config() const {
    Json arr = Json::array();
    for (const AwardRule& r : rules()) {
        Json j = Json::object();
        j.set("name", r.name).set("positions", r.positions).set("minPercent", r.minPercent);
        arr.push(j);
    }
    char tb[DSA_ID_LEN];
    awards_tiebreak(tb);
    Json out = Json::object();
    out.set("rules", arr).set("tiebreakKey", tb)
        .set("order", "Score, then the tie-break criterion, then attendance, then Delegate ID");
    return out;
}

Json RankingManager::setConfig(const Json& body) {
    const Json& list = body["rules"];
    if (!list.isArray() || list.size() < 1 || list.size() > MAX_AWARDS)
        throw ApiError(400, "VALIDATION", "Give between 1 and 8 awards.");
    std::vector<AwardRule> v;
    for (const Json& item : list.asArray()) {
        AwardRule r{};
        pers_copy_field(r.name, requireString(item, "name", AWARD_NAME_LEN - 1).c_str(), AWARD_NAME_LEN);
        r.positions = requireInt(item, "positions", 1, 50);
        r.minPercent = requireInt(item, "minPercent", 0, 100);
        v.push_back(r);
    }
    std::string tb = optionalString(body, "tiebreakKey", DSA_ID_LEN - 1, "");
    bool known = tb.empty();
    for (const CriterionRec& c : scoring_.criteria())
        if (tb == c.key) known = true;
    if (!known) throw ApiError(400, "VALIDATION", "The tie-break must be one of the scoring criteria.");
    int rc = awards_set(v.data(), static_cast<int>(v.size()), tb.c_str());
    if (rc == DSA_ERR_DUPLICATE) throw ApiError(400, "VALIDATION", "Two awards have the same name.");
    if (rc != DSA_OK) throw ApiError(400, "VALIDATION", "The award rules couldn't be saved.");
    save();
    return config();
}

std::vector<RankEntry> RankingManager::entries(const std::string& committeeId, const std::string& scope) const {
    std::vector<int> statuses = statusesFor(scope);
    char tb[DSA_ID_LEN];
    awards_tiebreak(tb);
    std::vector<RankEntry> out;
    std::vector<MarksheetRec> v(static_cast<std::size_t>(sheet_count()) + 1);
    int n = sheet_list(committeeId.c_str(), -1, v.data(), static_cast<int>(v.size()));
    for (int i = 0; i < n; ++i) {
        const MarksheetRec& r = v[static_cast<std::size_t>(i)];
        bool wanted = false;
        for (int s : statuses) wanted = wanted || r.status == s;
        if (!wanted) continue;
        Marksheet m(r);
        RankEntry e{};
        pers_copy_field(e.delegateId, r.delegateId, DSA_ID_LEN);
        pers_copy_field(e.committeeId, r.committeeId, DSA_ID_LEN);
        e.percentTenths = r.maxTotal ? static_cast<int>(std::lround(r.total * 1000.0 / r.maxTotal)) : 0;
        int tbMax = m.maxFor(tb);
        e.tiebreakTenths = tbMax ? static_cast<int>(std::lround(m.markFor(tb) * 1000.0 / tbMax)) : 0;
        e.attendanceTenths = static_cast<int>(std::lround(att_.delegateSummary(r.delegateId)["percentage"].asNumber() * 10));
        out.push_back(e);
    }
    return out;
}

Json RankingManager::rankingJson(const Ranking& r, bool withAwards) const {
    Json rows = Json::array();
    for (const RankedRow& row : r.rows()) {
        Json j = Json::object();
        CommitteeRec c;
        std::string abbr = committee_find(row.entry.committeeId, &c, nullptr) == DSA_OK ? c.abbreviation : row.entry.committeeId;
        j.set("rank", row.rank)
            .set("tied", row.tied)
            .set("separatedBy", row.separatedBy)
            .set("delegate", delegateBrief(row.entry.delegateId))
            .set("committee", abbr)
            .set("percent", row.entry.percentTenths / 10.0)
            .set("tiebreak", row.entry.tiebreakTenths / 10.0)
            .set("attendance", row.entry.attendanceTenths / 10.0);
        if (withAwards) j.set("award", row.award.empty() ? Json() : Json(row.award));
        rows.push(j);
    }
    Json sort = Json::object();
    sort.set("algorithm", Ranking::algorithmKey(r.algorithm()))
        .set("name", sort_algorithm_name(r.algorithm()))
        .set("n", static_cast<double>(r.rows().size()))
        .set("comparisons", static_cast<double>(r.stats().comparisons))
        .set("moves", static_cast<double>(r.stats().moves))
        .set("elapsedNs", static_cast<double>(r.elapsedNs()));
    Json out = Json::object();
    out.set("rows", rows).set("sort", sort).set("config", config());
    return out;
}

Json RankingManager::committeeRanking(const AuthContext& ctx, const std::string& committeeId,
                                      const std::string& algorithm, const std::string& scope) const {
    RoleManager::requireCommittee(ctx, committeeId, perm::VIEW);
    std::string sc = ctx.isEb() ? "all" : scope;  // EB sees their own committee's provisional order
    Committee c = conf_.getCommittee(committeeId);
    Ranking r(entries(committeeId, sc), Ranking::parseAlgorithm(algorithm));
    r.sortAndRank();
    r.assignAwards(rules());
    Json out = rankingJson(r, true);
    out.set("committee", c.toJson()).set("scope", sc.empty() ? "approved" : sc);
    return out;
}

Json RankingManager::overallRanking(const std::string& algorithm, const std::string& scope) const {
    Ranking r(entries("", scope), Ranking::parseAlgorithm(algorithm));
    r.sortAndRank();  // percentages compare fairly across committees
    Json out = rankingJson(r, false);
    out.set("scope", scope.empty() ? "approved" : scope);
    return out;
}

Json RankingManager::compare(const std::string& committeeId, const std::string& scope) const {
    std::vector<RankEntry> base = entries(committeeId, scope);
    Json arr = Json::array();
    std::vector<std::string> first;
    bool identical = true;
    for (int a = 0; a < SORT_ALGORITHM_COUNT; ++a) {
        Ranking r(base, a);
        r.sortAndRank();
        std::vector<std::string> order;
        for (const RankedRow& row : r.rows()) order.emplace_back(row.entry.delegateId);
        if (a == 0) first = order;
        else if (order != first) identical = false;
        Json j = Json::object();
        j.set("algorithm", Ranking::algorithmKey(a)).set("name", sort_algorithm_name(a))
            .set("comparisons", static_cast<double>(r.stats().comparisons))
            .set("moves", static_cast<double>(r.stats().moves))
            .set("movesAre", a == SORT_INSERTION || a == SORT_MERGE ? "element moves" : "swaps")
            .set("elapsedNs", static_cast<double>(r.elapsedNs()));
        arr.push(j);
    }
    Json out = Json::object();
    out.set("n", static_cast<double>(base.size())).set("results", arr).set("identicalRankings", identical)
        .set("data", "Real marksheets");
    return out;
}

// Runs the five algorithms on GENERATED entries (labelled as such), so the
// growth rates can be seen on sizes a single conference doesn't reach.
Json RankingManager::benchmark(int n, const std::string& order) const {
    if (n < 10 || n > 5000) throw ApiError(400, "VALIDATION", "Choose between 10 and 5000 entries.");
    if (order != "random" && order != "sorted" && order != "reversed")
        throw ApiError(400, "VALIDATION", "Order must be random, sorted or reversed.");
    std::mt19937 rng(2026);  // fixed seed: the same data every run
    std::vector<RankEntry> base(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        RankEntry& e = base[static_cast<std::size_t>(i)];
        e = RankEntry{};
        std::snprintf(e.delegateId, sizeof e.delegateId, "G%05d", i);
        e.percentTenths = order == "random" ? static_cast<int>(rng() % 1001) : order == "sorted" ? 1000 - (i * 1000 / n) : i * 1000 / n;
        e.tiebreakTenths = static_cast<int>(rng() % 1001);
        e.attendanceTenths = static_cast<int>(rng() % 1001);
    }
    if (order == "sorted") {  // exactly the order the ranking wants
        RankEntry* p = base.data();
        SortStats st;
        rank_sort(p, n, SORT_MERGE, &st);
    } else if (order == "reversed") {
        RankEntry* p = base.data();
        SortStats st;
        rank_sort(p, n, SORT_MERGE, &st);
        for (int i = 0, j = n - 1; i < j; ++i, --j) std::swap(base[static_cast<std::size_t>(i)], base[static_cast<std::size_t>(j)]);
    }
    Json arr = Json::array();
    for (int a = 0; a < SORT_ALGORITHM_COUNT; ++a) {
        Ranking r(base, a);
        r.sortAndRank();
        Json j = Json::object();
        j.set("algorithm", Ranking::algorithmKey(a)).set("name", sort_algorithm_name(a))
            .set("comparisons", static_cast<double>(r.stats().comparisons))
            .set("moves", static_cast<double>(r.stats().moves))
            .set("movesAre", a == SORT_INSERTION || a == SORT_MERGE ? "element moves" : "swaps")
            .set("elapsedNs", static_cast<double>(r.elapsedNs()));
        arr.push(j);
    }
    Json out = Json::object();
    out.set("n", n).set("order", order).set("results", arr)
        .set("data", "Generated test data with a fixed seed, not real delegates");
    return out;
}

Json RankingManager::myResults(const AuthContext& ctx) const {
    RoleManager::requireDelegate(ctx);
    Json out = Json::object();
    Json sheet = scoring_.mySheet(ctx);
    out.set("marksheet", sheet);
    DelegateRec d;
    if (sheet.isNull() || delegate_find(ctx.accountId.c_str(), &d, nullptr) != DSA_OK) {
        out.set("rank", Json()).set("award", Json()).set("committeeAwards", Json::array());
        return out;
    }
    // Ranked only among PUBLISHED results, so nothing unpublished leaks.
    Ranking r(entries(d.committeeId, "published"), SORT_MERGE);
    r.sortAndRank();
    r.assignAwards(rules());
    Json awards = Json::array();
    for (const RankedRow& row : r.rows()) {
        if (ctx.accountId == row.entry.delegateId)
            out.set("rank", row.rank).set("of", static_cast<double>(r.rows().size()))
                .set("award", row.award.empty() ? Json() : Json(row.award));
        if (!row.award.empty()) {
            Json a = Json::object();
            a.set("award", row.award).set("delegate", delegateBrief(row.entry.delegateId));
            awards.push(a);
        }
    }
    out.set("committeeAwards", awards);
    return out;
}

void registerRankingRoutes(Router& r, RankingManager& rm, Auth& auth) {
    r.get("/api/awards/config", [&rm, &auth](const HttpRequest& q) {
        auth.require(q);
        return HttpResponse::ok(rm.config()); });
    r.put("/api/awards/config", [&rm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(rm.setConfig(q.json())); });
    r.get("/api/committees/:id/rankings", [&rm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(rm.committeeRanking(auth.require(q), q.param("id"), q.queryParam("algorithm"),
                                                    q.queryParam("scope"))); });
    r.get("/api/rankings/overall", [&rm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(rm.overallRanking(q.queryParam("algorithm"), q.queryParam("scope"))); });
    r.get("/api/rankings/compare", [&rm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(rm.compare(q.queryParam("committeeId"), q.queryParam("scope"))); });
    r.get("/api/rankings/benchmark", [&rm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        std::string n = q.queryParam("n", "1000");
        if (n.empty() || n.size() > 5 || n.find_first_not_of("0123456789") != std::string::npos)
            throw ApiError(400, "VALIDATION", "n must be a number.");
        return HttpResponse::ok(rm.benchmark(std::stoi(n), q.queryParam("order", "random"))); });
    r.get("/api/delegate/me/results", [&rm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(rm.myResults(auth.require(q))); });
}

}  // namespace mun
