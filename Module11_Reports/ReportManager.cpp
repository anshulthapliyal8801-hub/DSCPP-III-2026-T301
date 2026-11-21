// MUNSphere — Module11_Reports/ReportManager.cpp
#include "ReportManager.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include "Database.h"
#include "DelegateManager.h"
#include "RoleManager.h"

namespace mun {

ReportManager::ReportManager(ConferenceManager& conf, AttendanceManager& att, ScoringManager& scoring,
                             RankingManager& ranking)
    : conf_(conf), att_(att), scoring_(scoring), ranking_(ranking), reports_(makeReports()) {}

const Report& ReportManager::find(const std::string& key) const {
    for (const auto& r : reports_)
        if (r->key() == key) return *r;
    throw ApiError(404, "NOT_FOUND", "No report called '" + key + "'.");
}

Json ReportManager::catalog(const AuthContext& ctx) const {
    Json arr = Json::array();
    for (const auto& r : reports_) {
        if (!ctx.isAdmin() && !(ctx.isEb() && r->committeeScoped())) continue;
        Json j = Json::object();
        j.set("key", r->key()).set("title", r->title()).set("description", r->description())
            .set("committeeScoped", r->committeeScoped()).set("requiresCommittee", r->requiresCommittee());
        arr.push(j);
    }
    return arr;
}

HttpResponse ReportManager::run(const AuthContext& ctx, const std::string& key, const std::string& rawCommittee,
                                const std::string& format) const {
    const Report& rep = find(key);  // virtual dispatch from here on
    std::string committeeId = rawCommittee;
    if (ctx.isEb()) {
        // EB: only committee-scoped reports, and only for their own committee.
        if (!rep.committeeScoped()) throw ApiError(403, "FORBIDDEN", "This report is for the Admin.");
        if (!committeeId.empty() && committeeId != ctx.committeeId)
            throw ApiError(403, "FORBIDDEN", "You can only report on your assigned committee.");
        RoleManager::requireCommittee(ctx, ctx.committeeId, perm::REPORTS);
        committeeId = ctx.committeeId;
    } else {
        RoleManager::requireAdmin(ctx);
    }
    if (rep.requiresCommittee() && committeeId.empty())
        throw ApiError(400, "VALIDATION", "Choose a committee for this report.");
    if (!committeeId.empty()) conf_.getCommittee(committeeId);  // 404 if unknown

    ReportContext rc{committeeId, conf_, att_, scoring_, ranking_};
    ReportTable t = rep.build(rc);
    std::string scope = committeeId.empty() ? "All committees" : conf_.getCommittee(committeeId).abbreviation();
    if (format.empty() || format == "json") return HttpResponse::ok(rep.toJson(t, scope));

    HttpResponse res;
    std::string stamp = nowTimestamp().substr(0, 10);
    std::string file = "munsphere-" + rep.key() + (committeeId.empty() ? "" : "-" + committeeId) + "-" + stamp;
    if (format == "csv") {
        res.contentType = "text/csv; charset=utf-8";
        res.body = "\xEF\xBB\xBF" + Report::toCsv(t);  // BOM so Excel reads UTF-8 names correctly
        file += ".csv";
    } else if (format == "txt") {
        res.contentType = "text/plain; charset=utf-8";
        res.body = rep.toText(t, scope);
        file += ".txt";
    } else {
        throw ApiError(400, "VALIDATION", "Format must be json, csv or txt.");
    }
    res.headers.emplace_back("Content-Disposition", "attachment; filename=\"" + file + "\"");
    return res;
}

// The DSA Analytics page: every search over every delegate, structure
// shapes, and the five sorts on real ranking data. All measured.
Json ReportManager::dsaAnalytics() const {
    std::vector<DelegateRec> dels(static_cast<std::size_t>(delegate_count()));
    dels.resize(static_cast<std::size_t>(delegate_to_array(dels.data(), static_cast<int>(dels.size()))));
    int n = static_cast<int>(dels.size());
    const char* names[4] = {"Hash table lookup", "Linear search", "Binary search", "BST search"};
    const char* keys[4] = {"hash", "linear", "binary", "bst"};
    const char* complexity[4] = {"O(1) average", "O(n)", "O(log n) after sorting", "O(h), h = tree height"};
    Json searches = Json::array();
    for (int k = 0; k < 4; ++k) {
        long long total = 0, ns = 0;
        int mx = 0, mn = n ? 1 << 30 : 0;
        for (const DelegateRec& d : dels) {
            SearchTrace tr;
            auto t0 = std::chrono::steady_clock::now();
            if (k == 0) delegate_search_hash(d.id, nullptr, &tr);
            else if (k == 1) delegate_search_linear_id(d.id, nullptr, &tr);
            else if (k == 2) delegate_search_binary_id(d.id, nullptr, &tr);
            else delegate_search_bst_name(d.name, nullptr, &tr);
            ns += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
            total += tr.comparisons;
            mx = std::max(mx, tr.comparisons);
            mn = std::min(mn, tr.comparisons);
        }
        Json j = Json::object();
        j.set("key", keys[k]).set("name", names[k]).set("complexity", complexity[k])
            .set("searches", n).set("averageComparisons", n ? static_cast<double>(total) / n : 0.0)
            .set("maxComparisons", mx).set("minComparisons", mn)
            .set("averageNs", n ? static_cast<double>(ns) / n : 0.0);
        searches.push(j);
    }
    int longest = 0;
    for (int b = 0; b < delegate_bucket_count(); ++b) longest = std::max(longest, delegate_bucket_length(b));
    int ideal = 0;
    while ((1 << ideal) - 1 < n) ideal++;
    Json structures = Json::object();
    structures.set("delegates", n)
        .set("hashBuckets", delegate_bucket_count()).set("loadFactor", delegate_load_factor())
        .set("longestChain", longest).set("rehashes", delegate_rehash_count())
        .set("bstHeight", bst_height()).set("bstMinimumHeight", ideal)
        .set("conferences", conference_count()).set("committees", committee_count()).set("sessions", session_count())
        .set("accounts", account_count()).set("ebMembers", eb_count()).set("ebCommitteeVertices", eb_committee_count())
        .set("attendanceEntries", att_total()).set("speeches", speech_count()).set("pois", poi_count())
        .set("marksheets", sheet_count()).set("auditEntries", audit_count());
    Json out = Json::object();
    out.set("searches", searches).set("structures", structures).set("sorting", ranking_.compare("", "all"))
        .set("note", "Linear and binary search times include copying the records into an array; binary search also sorts that copy first. Their comparison counts measure the search alone.");
    return out;
}

// --------------------------------------------------------- announcements

namespace {
const char* audienceKey(int a) {
    static const char* k[] = {"all", "delegates", "eb", "committee"};
    return a >= 0 && a <= 3 ? k[a] : "all";
}
}  // namespace

Json ReportManager::announcements(const AuthContext& ctx) const {
    std::vector<AnnouncementRec> v(static_cast<std::size_t>(ann_count()) + 1);
    int n = ann_list(v.data(), static_cast<int>(v.size()));
    std::string myCommittee = ctx.committeeId;
    if (ctx.isDelegate()) {
        DelegateRec d;
        if (delegate_find(ctx.accountId.c_str(), &d, nullptr) == DSA_OK) myCommittee = d.committeeId;
    }
    Json pinned = Json::array(), rest = Json::array();
    for (int i = 0; i < n; ++i) {
        const AnnouncementRec& a = v[static_cast<std::size_t>(i)];
        bool visible = ctx.isAdmin() || a.audience == AUD_ALL ||
                       (a.audience == AUD_DELEGATES && ctx.isDelegate()) || (a.audience == AUD_EB && ctx.isEb()) ||
                       (a.audience == AUD_COMMITTEE && !myCommittee.empty() && myCommittee == a.committeeId);
        if (!visible) continue;
        Json j = Json::object();
        j.set("id", a.id).set("title", a.title).set("body", a.body).set("audience", audienceKey(a.audience))
            .set("committeeId", a.committeeId).set("author", a.author).set("at", a.at).set("pinned", a.pinned == 1);
        (a.pinned ? pinned : rest).push(j);
    }
    for (const Json& j : rest.asArray()) pinned.push(j);  // pinned first, then newest first
    return pinned;
}

Json ReportManager::postAnnouncement(const AuthContext& ctx, const Json& body) {
    RoleManager::requireAdmin(ctx);
    AnnouncementRec a{};
    std::string title = requireString(body, "title", ANN_TITLE_LEN - 1);
    std::string text = requireString(body, "body", ANN_BODY_LEN - 1);
    std::string aud = optionalString(body, "audience", 20, "all");
    int audience = aud == "all" ? AUD_ALL : aud == "delegates" ? AUD_DELEGATES : aud == "eb" ? AUD_EB : aud == "committee" ? AUD_COMMITTEE : -1;
    if (audience < 0) throw ApiError(400, "VALIDATION", "Audience must be all, delegates, eb or committee.");
    if (audience == AUD_COMMITTEE) {
        std::string com = requireString(body, "committeeId", DSA_ID_LEN - 1);
        conf_.getCommittee(com);
        pers_copy_field(a.committeeId, com.c_str(), DSA_ID_LEN);
    }
    char id[DSA_ID_LEN + 8];
    std::snprintf(id, sizeof id, "AN%03d", ann_max_id_suffix("AN") + 1);
    pers_copy_field(a.id, id, DSA_ID_LEN);
    pers_copy_field(a.title, title.c_str(), ANN_TITLE_LEN);
    pers_copy_field(a.body, text.c_str(), ANN_BODY_LEN);
    a.audience = audience;
    pers_copy_field(a.author, ctx.accountId.c_str(), DSA_ID_LEN);
    pers_copy_field(a.at, nowTimestamp().c_str(), DSA_STAMP_LEN);
    a.pinned = body["pinned"].isBool() && body["pinned"].asBool() ? 1 : 0;
    if (ann_insert(&a) != DSA_OK) throw ApiError(500, "STORE", "The announcement couldn't be stored.");
    Database::instance().save(Store::Announcements);
    Json j = Json::object();
    j.set("id", a.id).set("title", a.title);
    return j;
}

void ReportManager::deleteAnnouncement(const std::string& id) {
    if (ann_delete(id.c_str()) != DSA_OK) throw ApiError(404, "NOT_FOUND", "No announcement with ID '" + id + "'.");
    Database::instance().save(Store::Announcements);
}

void registerReportRoutes(Router& r, ReportManager& rm, Auth& auth) {
    r.get("/api/reports", [&rm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(rm.catalog(auth.require(q))); });
    r.get("/api/reports/:key", [&rm, &auth](const HttpRequest& q) {
        return rm.run(auth.require(q), q.param("key"), q.queryParam("committeeId"), q.queryParam("format")); });
    r.get("/api/analytics/dsa", [&rm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(rm.dsaAnalytics()); });
    r.get("/api/announcements", [&rm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(rm.announcements(auth.require(q))); });
    r.post("/api/announcements", [&rm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(rm.postAnnouncement(auth.require(q), q.json()), 201); });
    r.del("/api/announcements/:id", [&rm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        rm.deleteAnnouncement(q.param("id"));
        return HttpResponse::ok(Json::object().set("deleted", q.param("id"))); });
}

}  // namespace mun
