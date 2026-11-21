// MUNSphere — Module8_Scoring/ScoringManager.cpp
#include "ScoringManager.h"

#include <cctype>
#include <cstdio>
#include <set>

#include "Database.h"
#include "DelegateManager.h"
#include "RoleManager.h"

namespace mun {

namespace {
void persist() { Database::instance().save(Store::Scoring); }

std::string slug(const std::string& name) {
    std::string s;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (!s.empty() && s.back() != '_') s += '_';
    }
    while (!s.empty() && s.back() == '_') s.pop_back();
    if (s.empty()) s = "criterion";
    return s.substr(0, 12);
}
}  // namespace

ScoringManager::ScoringManager(ConferenceManager& conf, AttendanceManager& att) : conf_(conf), att_(att) {
    ensureDefaults();
}

// The default criteria from the project specification (total 100).
void ScoringManager::ensureDefaults() {
    if (criteria_count() > 0) return;
    struct D { const char* key; const char* name; int max; int att; };
    const D defs[] = {{"research", "Research", 20, 0}, {"speaking", "Speaking", 20, 0},
                      {"participation", "Participation", 20, 0}, {"diplomacy", "Diplomacy", 15, 0},
                      {"poi_motion", "POI and Motion Quality", 15, 0}, {"attendance", "Attendance", 10, 1}};
    CriterionRec c[6];
    for (int i = 0; i < 6; ++i) {
        c[i] = CriterionRec{};
        pers_copy_field(c[i].key, defs[i].key, DSA_ID_LEN);
        pers_copy_field(c[i].name, defs[i].name, CRIT_NAME_LEN);
        c[i].maxMarks = defs[i].max;
        c[i].order = i + 1;
        c[i].fromAttendance = defs[i].att;
    }
    criteria_set(c, 6);
    if (Database::instance().isOpen()) persist();
}

std::vector<CriterionRec> ScoringManager::criteria() const {
    std::vector<CriterionRec> v(MAX_CRITERIA);
    v.resize(static_cast<std::size_t>(criteria_get(v.data(), MAX_CRITERIA)));
    return v;
}

Json ScoringManager::criteriaJson() const {
    Json arr = Json::array();
    int total = 0;
    for (const CriterionRec& c : criteria()) {
        Json j = Json::object();
        j.set("key", c.key).set("name", c.name).set("maxMarks", c.maxMarks).set("order", c.order)
            .set("fromAttendance", c.fromAttendance == 1);
        arr.push(j);
        total += c.maxMarks;
    }
    Json out = Json::object();
    out.set("criteria", arr).set("total", total);
    return out;
}

Json ScoringManager::setCriteria(const Json& body) {
    const Json& list = body["criteria"];
    if (!list.isArray() || list.size() < 1 || list.size() > MAX_CRITERIA)
        throw ApiError(400, "VALIDATION", "Give between 1 and 10 criteria.");
    std::vector<CriterionRec> v;
    std::set<std::string> keys, names;
    int order = 1, attendance = 0;
    for (const Json& item : list.asArray()) {
        CriterionRec c{};
        std::string name = requireString(item, "name", CRIT_NAME_LEN - 1);
        std::string lower = name;
        for (char& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (!names.insert(lower).second) throw ApiError(400, "VALIDATION", "Two criteria are both called '" + name + "'.");
        std::string key = item["key"].isString() && !item["key"].asString().empty() ? item["key"].asString() : slug(name);
        if (key.size() > DSA_ID_LEN - 1) key = key.substr(0, DSA_ID_LEN - 1);
        std::string base = key;
        for (int n = 2; keys.count(key); ++n) key = base.substr(0, 12) + std::to_string(n);
        keys.insert(key);
        pers_copy_field(c.key, key.c_str(), DSA_ID_LEN);
        pers_copy_field(c.name, name.c_str(), CRIT_NAME_LEN);
        c.maxMarks = requireInt(item, "maxMarks", 1, 100);
        c.order = order++;
        c.fromAttendance = item["fromAttendance"].isBool() && item["fromAttendance"].asBool() ? 1 : 0;
        attendance += c.fromAttendance;
        v.push_back(c);
    }
    if (attendance > 1) throw ApiError(400, "VALIDATION", "Only one criterion can be filled from attendance.");
    if (criteria_set(v.data(), static_cast<int>(v.size())) != DSA_OK)
        throw ApiError(400, "VALIDATION", "The criteria couldn't be saved.");
    persist();
    return criteriaJson();
}

Marksheet ScoringManager::load(const std::string& id) const {
    MarksheetRec r;
    if (sheet_find(id.c_str(), &r, nullptr) != DSA_OK) throw ApiError(404, "NOT_FOUND", "No marksheet with ID '" + id + "'.");
    return Marksheet(r);
}

void ScoringManager::store(Marksheet& m, int action, const std::string& by, const std::string& note, bool isNew) {
    if (!isNew) m.stamp(by);
    MarksheetRec r = m.record();
    int rc = isNew ? sheet_insert(&r) : sheet_update(&r);
    if (rc == DSA_ERR_DUPLICATE) throw ApiError(409, "DUPLICATE", "This delegate already has a marksheet.");
    if (rc != DSA_OK) throw ApiError(500, "STORE", "The marksheet couldn't be stored.");
    SheetRevisionRec v = m.revision(action, by, note);
    rev_add(&v);  // revision history: every action, with a snapshot of the marks
    persist();
}

// Writing marks belongs to the committee's EB with the evaluate permission.
// Admin reviews but never writes marks (separation of duties).
void ScoringManager::requireWriter(const AuthContext& ctx, const std::string& committeeId) const {
    if (!ctx.isEb())
        throw ApiError(403, "FORBIDDEN", "Marks are entered by the committee's Executive Board.");
    RoleManager::requireCommittee(ctx, committeeId, perm::EVALUATE);
}

Json ScoringManager::committeeSheets(const AuthContext& ctx, const std::string& committeeId) const {
    RoleManager::requireCommittee(ctx, committeeId, perm::VIEW);
    conf_.getCommittee(committeeId);
    Json rows = Json::array();
    for (const DelegateRec& d : delegatesInCommittee(committeeId)) {
        Json j = Json::object();
        j.set("delegate", delegateBrief(d.id));
        MarksheetRec r;
        if (sheet_find_by_delegate(d.id, &r) == DSA_OK) {
            Marksheet m(r);
            j.set("marksheetId", r.id).set("status", m.statusKey(r.status)).set("total", r.total)
                .set("maxTotal", r.maxTotal).set("percent", m.percent()).set("adminNote", r.adminNote);
        } else {
            j.set("marksheetId", Json()).set("status", "none");
        }
        rows.push(j);
    }
    return rows;
}

Json ScoringManager::create(const AuthContext& ctx, const std::string& committeeId, const Json& body) {
    requireWriter(ctx, committeeId);
    std::string delegateId = requireString(body, "delegateId", DSA_ID_LEN - 1);
    DelegateRec d;
    if (delegate_find(delegateId.c_str(), &d, nullptr) != DSA_OK || committeeId != d.committeeId)
        throw ApiError(400, "VALIDATION", delegateId + " isn't a delegate of this committee.");
    if (sheet_find_by_delegate(delegateId.c_str(), nullptr) == DSA_OK)
        throw ApiError(409, "DUPLICATE", "This delegate already has a marksheet.");
    double attPct = att_.delegateSummary(delegateId)["percentage"].asNumber();
    char id[DSA_ID_LEN + 8];
    std::snprintf(id, sizeof id, "MS%03d", sheet_max_id_suffix("MS") + 1);
    Marksheet m = Marksheet::create(id, delegateId, committeeId, ctx.accountId, criteria(), attPct);
    store(m, SHEET_ACT_CREATED, ctx.accountId, "", true);
    return m.toJson(false);
}

Json ScoringManager::get(const AuthContext& ctx, const std::string& id) const {
    Marksheet m = load(id);
    const MarksheetRec& r = m.record();
    if (ctx.isDelegate()) {
        if (ctx.accountId != r.delegateId || r.status != SHEET_PUBLISHED)
            throw ApiError(404, "NOT_FOUND", "No published marksheet found.");  // no hint about others' sheets
        return m.toJson(true);
    }
    RoleManager::requireCommittee(ctx, r.committeeId, perm::VIEW);
    Json j = m.toJson(false);
    SheetRevisionRec revs[100];
    int n = rev_list(r.id, revs, 100);
    Json hist = Json::array();
    for (int i = 0; i < n; ++i) {
        Json h = Json::object();
        Json marks = Json::array();
        for (int k = 0; k < revs[i].marksCount; ++k) marks.push(revs[i].marks[k]);
        h.set("revision", revs[i].revision).set("action", Marksheet::actionKey(revs[i].action))
            .set("by", revs[i].by).set("at", revs[i].at).set("total", revs[i].total).set("marks", marks).set("note", revs[i].note);
        hist.push(h);
    }
    j.set("history", hist);
    return j;
}

Json ScoringManager::save(const AuthContext& ctx, const std::string& id, const Json& body) {
    Marksheet m = load(id);
    requireWriter(ctx, m.record().committeeId);
    if (body.has("marks")) m.setMarks(body["marks"]);
    if (body.has("remarks")) m.setRemarks(optionalString(body, "remarks", DSA_TEXT_LEN - 1, ""));
    if (!m.editable()) throw ApiError(409, "INVALID_STATE", "This marksheet can't be edited now.");
    store(m, SHEET_ACT_SAVED, ctx.accountId, "", false);
    return get(ctx, id);
}

Json ScoringManager::submit(const AuthContext& ctx, const std::string& id) {
    Marksheet m = load(id);
    requireWriter(ctx, m.record().committeeId);
    m.submit();
    store(m, SHEET_ACT_SUBMITTED, ctx.accountId, "", false);
    return get(ctx, id);
}

Json ScoringManager::review(const HttpRequest& q) const {
    std::string status = q.queryParam("status"), committee = q.queryParam("committeeId");
    int st = -1;
    for (int s = 0; s < SHEET_STATUS_COUNT; ++s)
        if (status == Marksheet::statusKey(s)) st = s;
    if (!status.empty() && st < 0) throw ApiError(400, "VALIDATION", "Unknown marksheet status.");
    std::vector<MarksheetRec> v(static_cast<std::size_t>(sheet_count()) + 1);
    int n = sheet_list(committee.c_str(), st, v.data(), static_cast<int>(v.size()));
    Json arr = Json::array();
    for (int i = 0; i < n; ++i) arr.push(Marksheet(v[static_cast<std::size_t>(i)]).toJson(false));
    return arr;
}

Json ScoringManager::approve(const AuthContext& ctx, const std::string& id) {
    RoleManager::requireAdmin(ctx);  // EB members can never approve, including their own sheets
    Marksheet m = load(id);
    m.approve();
    store(m, SHEET_ACT_APPROVED, ctx.accountId, "", false);
    return get(ctx, id);
}

Json ScoringManager::returnSheet(const AuthContext& ctx, const std::string& id, const Json& body) {
    RoleManager::requireAdmin(ctx);
    Marksheet m = load(id);
    std::string note = body["note"].isString() ? body["note"].asString() : "";
    m.returnForCorrection(note);
    store(m, SHEET_ACT_RETURNED, ctx.accountId, validate::trim(note), false);
    return get(ctx, id);
}

Json ScoringManager::publish(const AuthContext& ctx, const std::string& id) {
    RoleManager::requireAdmin(ctx);
    Marksheet m = load(id);
    m.publish();
    store(m, SHEET_ACT_PUBLISHED, ctx.accountId, "", false);
    return get(ctx, id);
}

Json ScoringManager::publishCommittee(const AuthContext& ctx, const std::string& committeeId) {
    RoleManager::requireAdmin(ctx);
    conf_.getCommittee(committeeId);
    std::vector<MarksheetRec> v(static_cast<std::size_t>(sheet_count()) + 1);
    int n = sheet_list(committeeId.c_str(), SHEET_APPROVED, v.data(), static_cast<int>(v.size()));
    for (int i = 0; i < n; ++i) {
        Marksheet m(v[static_cast<std::size_t>(i)]);
        m.publish();
        store(m, SHEET_ACT_PUBLISHED, ctx.accountId, "", false);
    }
    Json j = Json::object();
    j.set("published", n);
    return j;
}

Json ScoringManager::mySheet(const AuthContext& ctx) const {
    RoleManager::requireDelegate(ctx);
    MarksheetRec r;
    if (sheet_find_by_delegate(ctx.accountId.c_str(), &r) != DSA_OK || r.status != SHEET_PUBLISHED) return Json();
    return Marksheet(r).toJson(true);
}

void registerScoringRoutes(Router& r, ScoringManager& sm, Auth& auth) {
    r.get("/api/scoring/criteria", [&sm, &auth](const HttpRequest& q) {
        auth.require(q);
        return HttpResponse::ok(sm.criteriaJson()); });
    r.put("/api/scoring/criteria", [&sm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(sm.setCriteria(q.json())); });
    r.get("/api/committees/:id/marksheets", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.committeeSheets(auth.require(q), q.param("id"))); });
    r.post("/api/committees/:id/marksheets", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.create(auth.require(q), q.param("id"), q.json()), 201); });
    r.post("/api/committees/:id/marksheets/publish", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.publishCommittee(auth.require(q), q.param("id"))); });
    r.get("/api/marksheets", [&sm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(sm.review(q)); });
    r.get("/api/marksheets/:id", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.get(auth.require(q), q.param("id"))); });
    r.put("/api/marksheets/:id", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.save(auth.require(q), q.param("id"), q.json())); });
    r.post("/api/marksheets/:id/submit", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.submit(auth.require(q), q.param("id"))); });
    r.post("/api/marksheets/:id/approve", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.approve(auth.require(q), q.param("id"))); });
    r.post("/api/marksheets/:id/return", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.returnSheet(auth.require(q), q.param("id"), q.json())); });
    r.post("/api/marksheets/:id/publish", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.publish(auth.require(q), q.param("id"))); });
    r.get("/api/delegate/me/marksheet", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.mySheet(auth.require(q))); });
}

}  // namespace mun
