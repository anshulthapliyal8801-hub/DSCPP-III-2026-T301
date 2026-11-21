// MUNSphere — Module5_SpeakerList/LiveSession.cpp
#include "LiveSession.h"

#include "Attendance.h"
#include "DelegateManager.h"
#include "RoleManager.h"

namespace mun {

namespace {

// The committee whose active session this user should see (EB or delegate).
std::string myCommittee(const AuthContext& ctx) {
    if (ctx.isEb()) return ctx.committeeId;
    if (ctx.isDelegate()) {
        DelegateRec d;
        if (delegate_find(ctx.accountId.c_str(), &d, nullptr) == DSA_OK) return d.committeeId;
    }
    return {};
}

}  // namespace

void registerLiveRoutes(Router& r, Auth& auth, ConferenceManager& conf, AttendanceManager& att,
                        SpeakerManager& speakers, MotionManager& motions, VotingManager& votes) {
    // Which session is live for me right now? (EB and delegates)
    r.get("/api/live/current", [&auth, &conf](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        std::string committeeId = myCommittee(ctx);
        Json out = Json::object();
        out.set("committeeId", committeeId).set("sessionId", Json());
        if (!committeeId.empty()) {
            std::vector<Session> active = conf.listSessions(committeeId, "active");
            if (!active.empty()) out.set("sessionId", active.front().id());
        }
        return HttpResponse::ok(out);
    });

    r.get("/api/sessions/:id/live", [&](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        Session s = conf.getSession(q.param("id"));
        bool full = RoleManager::canCommittee(ctx, s.committeeId(), perm::VIEW);
        if (!full && !(ctx.isDelegate() && myCommittee(ctx) == s.committeeId()))
            throw ApiError(403, "FORBIDDEN", "This session belongs to another committee.");

        Json viewer = Json::object();
        viewer.set("id", ctx.accountId).set("role", RoleManager::roleKey(ctx.role))
            .set("permissions", ctx.isAdmin() ? RoleManager::permissionsToJson(perm::ALL)
                                              : RoleManager::permissionsToJson(ctx.permissions));

        Json attendance = Json::object();
        if (full) {
            attendance = att.sheet(s.id());
        } else {
            AttendanceRec a;
            bool marked = att_find(s.id().c_str(), ctx.accountId.c_str(), &a, nullptr) == DSA_OK;
            attendance.set("mine", marked ? Json(AttendanceEntry::statusKey(a.status)) : Json("unmarked"));
            attendance.set("seated", att.isSeated(s.id(), ctx.accountId));
        }
        // Placards in the room: every delegate sees who is in their committee.
        Json roster = Json::array();
        for (const DelegateRec& d : delegatesInCommittee(s.committeeId())) {
            Json j = Json::object();
            j.set("id", d.id).set("name", d.name).set("portfolio", d.portfolio);
            roster.push(j);
        }

        Json out = Json::object();
        out.set("session", s.toJson())
            .set("committee", conf.getCommittee(s.committeeId()).toJson())
            .set("serverNow", static_cast<double>(nowEpoch()))
            .set("viewer", viewer)
            .set("roster", roster)
            .set("attendance", attendance)
            .set("speakers", speakers.state(s.id(), ctx, full))
            .set("motions", motions.state(s.id()))
            .set("vote", votes.state(s.id(), ctx, full));
        return HttpResponse::ok(out);
    });

    // Admin: every session in progress, one line each.
    r.get("/api/live/overview", [&auth, &conf, &speakers, &motions, &votes](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireAdmin(ctx);
        Json arr = Json::array();
        for (const Session& s : conf.listSessions("", "active")) {
            Json sp = speakers.state(s.id(), ctx, true);
            Json mo = motions.state(s.id());
            Json vo = votes.state(s.id(), ctx, true);
            Json j = Json::object();
            j.set("session", s.toJson())
                .set("committee", conf.getCommittee(s.committeeId()).toJson())
                .set("serverNow", static_cast<double>(nowEpoch()))
                .set("current", sp["current"])
                .set("startedAt", sp["startedAt"])
                .set("speakingSeconds", sp["speakingSeconds"])
                .set("queueLength", sp["queue"].size())
                .set("requests", sp["requestCount"])
                .set("poisPending", sp["poisPendingCount"])
                .set("motionsPending", mo["pending"].size())
                .set("onFloor", mo["onFloor"])
                .set("openVote", vo["open"]);
            arr.push(j);
        }
        return HttpResponse::ok(arr);
    });
}

}  // namespace mun
