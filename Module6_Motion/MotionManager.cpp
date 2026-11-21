// MUNSphere — Module6_Motion/MotionManager.cpp
#include "MotionManager.h"

#include <cstdio>

#include "Database.h"
#include "DelegateManager.h"
#include "RoleManager.h"

namespace mun {

namespace { void save() { Database::instance().save(Store::Motions); } }

MotionManager::MotionManager(ConferenceManager& conf, AttendanceManager& att, VotingManager& votes,
                             SpeakerManager& speakers)
    : conf_(conf), att_(att), votes_(votes), speakers_(speakers) {
    votes_.onClose([this](const VoteRec& v) { applyResult(v); });
}

Motion MotionManager::load(const std::string& id) const {
    MotionRec r;
    if (motion_find(id.c_str(), &r) != DSA_OK) throw ApiError(404, "NOT_FOUND", "No motion with ID '" + id + "'.");
    return Motion(r);
}

void MotionManager::store(const Motion& m) {
    MotionRec r = m.record();
    if (motion_update(&r) != DSA_OK) throw ApiError(500, "STORE", "The motion couldn't be updated.");
    save();
}

void MotionManager::pushAction(const Motion& m, int from, int to, const std::string& by) {
    MotionActionRec a{};
    pers_copy_field(a.sessionId, m.record().sessionId, DSA_ID_LEN);
    pers_copy_field(a.motionId, m.id().c_str(), DSA_ID_LEN);
    a.fromStatus = from;
    a.toStatus = to;
    pers_copy_field(a.by, by.c_str(), DSA_ID_LEN);
    pers_copy_field(a.at, nowTimestamp().c_str(), DSA_STAMP_LEN);
    mact_push(&a);
}

Json MotionManager::propose(const AuthContext& ctx, const std::string& sessionId, const Json& body) {
    Session s = conf_.getSession(sessionId);
    std::string proposer;
    if (ctx.isDelegate()) {
        att_.requireSeated(ctx, s);
        proposer = ctx.accountId;
    } else {
        // The chair can enter a motion raised on the floor, on a delegate's behalf.
        RoleManager::requireCommittee(ctx, s.committeeId(), perm::MOTIONS);
        AttendanceManager::requireActive(s);
        proposer = requireString(body, "delegateId", DSA_ID_LEN - 1);
        if (!att_.isSeated(sessionId, proposer))
            throw ApiError(409, "NOT_SEATED", proposer + " isn't marked present in this session.");
    }
    char id[DSA_ID_LEN + 8];
    std::snprintf(id, sizeof id, "M%03d", motion_max_id_suffix("M") + 1);
    Motion m = Motion::create(id, sessionId, s.committeeId(), proposer, requireString(body, "type", 30), body);
    MotionRec r = m.record();
    if (motion_insert(&r) != DSA_OK) throw ApiError(500, "STORE", "The motion couldn't be stored.");
    save();
    return m.toJson();
}

Json MotionManager::withdraw(const AuthContext& ctx, const std::string& motionId) {
    Motion m = load(motionId);
    if (!(ctx.isDelegate() && ctx.accountId == m.record().proposerId))
        throw ApiError(403, "FORBIDDEN", "Only the delegate who proposed a motion can withdraw it.");
    if (m.status() != MS_PENDING) throw ApiError(409, "INVALID_STATE", "Only a pending motion can be withdrawn.");
    m.setStatus(MS_WITHDRAWN);
    store(m);  // leaves the priority queue
    return m.toJson();
}

Json MotionManager::ruleOutOfOrder(const AuthContext& ctx, const std::string& motionId) {
    Motion m = load(motionId);
    RoleManager::requireCommittee(ctx, m.record().committeeId, perm::MOTIONS);
    if (m.status() != MS_PENDING) throw ApiError(409, "INVALID_STATE", "Only a pending motion can be ruled out of order.");
    m.setStatus(MS_OUT_OF_ORDER);
    store(m);
    pushAction(m, MS_PENDING, MS_OUT_OF_ORDER, ctx.accountId);
    save();
    return m.toJson();
}

Json MotionManager::takeNextToVote(const AuthContext& ctx, const std::string& sessionId) {
    Session s = conf_.getSession(sessionId);
    RoleManager::requireCommittee(ctx, s.committeeId(), perm::MOTIONS);
    AttendanceManager::requireActive(s);
    MotionRec top;
    if (motion_peek(sessionId.c_str(), &top) != DSA_OK)  // highest precedence first
        throw ApiError(409, "NO_MOTIONS", "There are no pending motions.");
    Motion m(top);
    std::string voteId = votes_.openProcedural(sessionId, s.committeeId(), "Motion " + m.id() + ": " + m.describe(),
                                               m.id(), Motion::info(m.type()).majority);
    m.setStatus(MS_VOTING);
    m.setVote(voteId);
    store(m);
    pushAction(m, MS_PENDING, MS_VOTING, ctx.accountId);
    save();
    return m.toJson();
}

// Undo reverses the most recent chair action (top of the stack), but
// only while it still holds: a motion whose vote has received ballots
// or finished can't go back.
Json MotionManager::undo(const AuthContext& ctx, const std::string& sessionId) {
    Session s = conf_.getSession(sessionId);
    RoleManager::requireCommittee(ctx, s.committeeId(), perm::MOTIONS);
    MotionActionRec a;
    if (mact_peek(sessionId.c_str(), &a) != DSA_OK) throw ApiError(409, "NOTHING_TO_UNDO", "There is nothing to undo.");
    Motion m = load(a.motionId);
    if (m.status() != a.toStatus)
        throw ApiError(409, "CANT_UNDO", "The last action on " + m.id() + " can't be undone: the motion has moved on (" +
                                             Motion::statusKey(m.status()) + ").");
    if (a.toStatus == MS_VOTING) {
        if (!votes_.discardIfEmpty(m.record().voteId))
            throw ApiError(409, "CANT_UNDO", "Ballots have already been cast on " + m.id() + ", so it can't be undone.");
        m.setVote("");
    }
    mact_pop(sessionId.c_str(), &a);
    m.setStatus(a.fromStatus);  // back into the priority queue
    store(m);
    Json j = Json::object();
    j.set("undone", Motion::statusKey(a.toStatus)).set("motion", m.toJson());
    return j;
}

void MotionManager::applyResult(const VoteRec& v) {
    if (!v.motionId[0]) return;
    MotionRec r;
    if (motion_find(v.motionId, &r) != DSA_OK || r.status != MS_VOTING) return;
    Motion m(r);
    bool passed = v.outcome == 1;
    m.setStatus(passed ? MS_PASSED : MS_FAILED);
    store(m);
    // A passed motion takes effect.
    if (passed && m.type() == MOT_SPEAKING_TIME) speakers_.applySpeakingTime(r.sessionId, r.speakingSeconds);
}

Json MotionManager::state(const std::string& sessionId) const {
    MotionRec buf[100];
    int n = motion_pending(sessionId.c_str(), buf, 100);  // priority order from the heap
    Json pending = Json::array(), recent = Json::array();
    for (int i = 0; i < n; ++i) pending.push(Motion(buf[i]).toJson());
    Json onFloor;
    n = motion_list(sessionId.c_str(), buf, 100);
    for (int i = 0; i < n; ++i) {
        if (buf[i].status == MS_VOTING) onFloor = Motion(buf[i]).toJson();
        else if (buf[i].status != MS_PENDING && recent.size() < 6) recent.push(Motion(buf[i]).toJson());
    }
    Json undo;
    MotionActionRec a;
    if (mact_peek(sessionId.c_str(), &a) == DSA_OK) {
        undo = Json::object();
        undo.set("motionId", a.motionId).set("action", a.toStatus == MS_VOTING ? "put to a vote" : "ruled out of order");
    }
    Json out = Json::object();
    out.set("pending", pending).set("onFloor", onFloor).set("recent", recent).set("undo", undo)
        .set("undoDepth", mact_depth(sessionId.c_str())).set("types", Motion::typeCatalog());
    return out;
}

void registerMotionRoutes(Router& r, MotionManager& mm, Auth& auth) {
    r.get("/api/motion-types", [&auth](const HttpRequest& q) {
        auth.require(q);
        return HttpResponse::ok(Motion::typeCatalog()); });
    r.post("/api/sessions/:id/motions", [&mm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(mm.propose(auth.require(q), q.param("id"), q.json()), 201); });
    r.post("/api/sessions/:id/motions/next", [&mm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(mm.takeNextToVote(auth.require(q), q.param("id"))); });
    r.post("/api/sessions/:id/motions/undo", [&mm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(mm.undo(auth.require(q), q.param("id"))); });
    r.post("/api/motions/:id/withdraw", [&mm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(mm.withdraw(auth.require(q), q.param("id"))); });
    r.post("/api/motions/:id/out-of-order", [&mm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(mm.ruleOutOfOrder(auth.require(q), q.param("id"))); });
}

}  // namespace mun
