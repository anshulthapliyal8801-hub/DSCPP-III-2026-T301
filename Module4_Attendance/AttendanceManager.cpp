// MUNSphere — Module4_Attendance/AttendanceManager.cpp
#include "AttendanceManager.h"

#include <vector>

#include "Attendance.h"
#include "Database.h"
#include "DelegateManager.h"
#include "RoleManager.h"

namespace mun {

AttendanceManager::AttendanceManager(ConferenceManager& conferences) : conf_(conferences) {}

void AttendanceManager::requireActive(const Session& s) {
    if (s.status() != SESSION_ACTIVE)
        throw ApiError(409, "SESSION_NOT_ACTIVE", "This session isn't in progress.");
}

bool AttendanceManager::isSeated(const std::string& sessionId, const std::string& delegateId) const {
    AttendanceRec a;
    if (att_find(sessionId.c_str(), delegateId.c_str(), &a, nullptr) != DSA_OK) return false;
    return AttendanceEntry(a).attended();
}

void AttendanceManager::requireSeated(const AuthContext& ctx, const Session& s) const {
    RoleManager::requireDelegate(ctx);
    DelegateRec d;
    if (delegate_find(ctx.accountId.c_str(), &d, nullptr) != DSA_OK || s.committeeId() != d.committeeId)
        throw ApiError(403, "FORBIDDEN", "You aren't a delegate of this committee.");
    requireActive(s);
    if (!isSeated(s.id(), ctx.accountId))
        throw ApiError(409, "NOT_SEATED", "The Executive Board hasn't marked you present yet. Check in first.");
}

Json AttendanceManager::sheet(const std::string& sessionId) const {
    Session s = conf_.getSession(sessionId);
    int counts[ATT_STATUS_COUNT];
    att_session_tally(sessionId.c_str(), counts);
    Json rows = Json::array();
    int unmarked = 0;
    for (const DelegateRec& d : delegatesInCommittee(s.committeeId())) {
        AttendanceRec a;
        Json j = Json::object();
        j.set("delegateId", d.id).set("name", d.name).set("portfolio", d.portfolio);
        if (att_find(sessionId.c_str(), d.id, &a, nullptr) == DSA_OK) {
            j.set("status", AttendanceEntry::statusKey(a.status))
                .set("statusLabel", AttendanceEntry::statusLabel(a.status))
                .set("markedAt", a.markedAt);
        } else {
            j.set("status", "unmarked").set("statusLabel", "Not marked");
            unmarked++;
        }
        rows.push(j);
    }
    Json tally = Json::object();
    for (int k = 0; k < ATT_STATUS_COUNT; ++k) tally.set(AttendanceEntry::statusKey(k), counts[k]);
    tally.set("unmarked", unmarked);
    Json out = Json::object();
    out.set("session", s.toJson()).set("tally", tally).set("delegates", rows);
    return out;
}

static void storeMark(const std::string& sessionId, const std::string& delegateId, int status,
                      const std::string& by) {
    AttendanceRec a{};
    pers_copy_field(a.sessionId, sessionId.c_str(), DSA_ID_LEN);
    pers_copy_field(a.delegateId, delegateId.c_str(), DSA_ID_LEN);
    a.status = status;
    pers_copy_field(a.markedBy, by.c_str(), DSA_ID_LEN);
    pers_copy_field(a.markedAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
    if (att_mark(&a, nullptr) != DSA_OK) throw ApiError(500, "STORE", "Attendance couldn't be stored.");
}

Json AttendanceManager::mark(const AuthContext& ctx, const std::string& sessionId, const Json& body) {
    Session s = conf_.getSession(sessionId);
    RoleManager::requireCommittee(ctx, s.committeeId(), perm::ATTENDANCE);
    if (s.status() == SESSION_SCHEDULED)
        throw ApiError(409, "SESSION_NOT_STARTED", "Attendance opens when the session is activated.");
    std::string delegateId = requireString(body, "delegateId", DSA_ID_LEN - 1);
    int status = AttendanceEntry::parseStatus(requireString(body, "status", 20), false);
    DelegateRec d;
    if (delegate_find(delegateId.c_str(), &d, nullptr) != DSA_OK || s.committeeId() != d.committeeId)
        throw ApiError(400, "VALIDATION", delegateId + " isn't a delegate of this committee.");
    storeMark(sessionId, delegateId, status, ctx.accountId);  // update, never a duplicate
    Database::instance().save(Store::Attendance);
    return sheet(sessionId);
}

Json AttendanceManager::markAll(const AuthContext& ctx, const std::string& sessionId, const Json& body) {
    Session s = conf_.getSession(sessionId);
    RoleManager::requireCommittee(ctx, s.committeeId(), perm::ATTENDANCE);
    if (s.status() == SESSION_SCHEDULED)
        throw ApiError(409, "SESSION_NOT_STARTED", "Attendance opens when the session is activated.");
    int status = AttendanceEntry::parseStatus(requireString(body, "status", 20), false);
    for (const DelegateRec& d : delegatesInCommittee(s.committeeId())) {
        AttendanceRec a;
        bool marked = att_find(sessionId.c_str(), d.id, &a, nullptr) == DSA_OK && a.status != ATT_CHECKED_IN;
        if (!marked) storeMark(sessionId, d.id, status, ctx.accountId);  // explicit marks are kept
    }
    Database::instance().save(Store::Attendance);
    return sheet(sessionId);
}

Json AttendanceManager::checkIn(const AuthContext& ctx, const std::string& sessionId) {
    RoleManager::requireDelegate(ctx);
    Session s = conf_.getSession(sessionId);
    DelegateRec d;
    if (delegate_find(ctx.accountId.c_str(), &d, nullptr) != DSA_OK || s.committeeId() != d.committeeId)
        throw ApiError(403, "FORBIDDEN", "You aren't a delegate of this committee.");
    requireActive(s);
    AttendanceRec a;
    if (att_find(sessionId.c_str(), ctx.accountId.c_str(), &a, nullptr) == DSA_OK)
        throw ApiError(409, "ALREADY_MARKED", "You're already on today's sheet as " +
                                                  AttendanceEntry::statusLabel(a.status) + ".");
    storeMark(sessionId, ctx.accountId, ATT_CHECKED_IN, ctx.accountId);
    Database::instance().save(Store::Attendance);
    return delegateSummary(ctx.accountId);
}

Json AttendanceManager::delegateSummary(const std::string& delegateId) const {
    DelegateRec d;
    if (delegate_find(delegateId.c_str(), &d, nullptr) != DSA_OK)
        throw ApiError(404, "NOT_FOUND", "No delegate with ID '" + delegateId + "'.");
    Json rows = Json::array();
    int held = 0, attended = 0;
    if (d.committeeId[0]) {
        for (const Session& s : conf_.listSessions(d.committeeId, "")) {
            if (s.status() == SESSION_SCHEDULED) continue;  // only sessions that took place
            held++;
            AttendanceRec a;
            Json j = Json::object();
            j.set("sessionId", s.id()).set("title", s.title()).set("date", s.date()).set("sessionStatus", s.toJson()["status"]);
            if (att_find(s.id().c_str(), delegateId.c_str(), &a, nullptr) == DSA_OK) {
                AttendanceEntry e(a);
                if (e.attended()) attended++;
                j.set("status", AttendanceEntry::statusKey(a.status)).set("statusLabel", AttendanceEntry::statusLabel(a.status));
            } else {
                j.set("status", "unmarked").set("statusLabel", "Not marked");
            }
            rows.push(j);
        }
    }
    Json out = Json::object();
    out.set("sessionsHeld", held)
        .set("attended", attended)
        .set("percentage", held ? (attended * 1000 / held) / 10.0 : 0.0)
        .set("sessions", rows);
    return out;
}

Json AttendanceManager::committeeReport(const std::string& committeeId) const {
    conf_.getCommittee(committeeId);
    Json rows = Json::array();
    for (const DelegateRec& d : delegatesInCommittee(committeeId)) {
        Json s = delegateSummary(d.id);
        Json j = Json::object();
        j.set("delegateId", d.id).set("name", d.name).set("portfolio", d.portfolio)
            .set("sessionsHeld", s["sessionsHeld"]).set("attended", s["attended"]).set("percentage", s["percentage"]);
        rows.push(j);
    }
    return rows;
}

void registerAttendanceRoutes(Router& r, AttendanceManager& am, ConferenceManager& conf, Auth& auth) {
    r.get("/api/sessions/:id/attendance", [&am, &conf, &auth](const HttpRequest& q) {
        RoleManager::requireCommittee(auth.require(q), conf.getSession(q.param("id")).committeeId(), perm::VIEW);
        return HttpResponse::ok(am.sheet(q.param("id")));
    });
    r.post("/api/sessions/:id/attendance", [&am, &auth](const HttpRequest& q) {
        return HttpResponse::ok(am.mark(auth.require(q), q.param("id"), q.json()));
    });
    r.post("/api/sessions/:id/attendance/all", [&am, &auth](const HttpRequest& q) {
        return HttpResponse::ok(am.markAll(auth.require(q), q.param("id"), q.json()));
    });
    r.post("/api/sessions/:id/check-in", [&am, &auth](const HttpRequest& q) {
        return HttpResponse::ok(am.checkIn(auth.require(q), q.param("id")));
    });
    r.get("/api/delegate/me/attendance", [&am, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireDelegate(ctx);
        return HttpResponse::ok(am.delegateSummary(ctx.accountId));
    });
    r.get("/api/committees/:id/attendance-report", [&am, &auth](const HttpRequest& q) {
        RoleManager::requireCommittee(auth.require(q), q.param("id"), perm::VIEW);
        return HttpResponse::ok(am.committeeReport(q.param("id")));
    });
}

}  // namespace mun
