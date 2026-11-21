// MUNSphere — Module5_SpeakerList/SpeakerManager.cpp
#include "SpeakerManager.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "Database.h"
#include "DelegateManager.h"
#include "RoleManager.h"
#include "Speaker.h"

namespace mun {

namespace {
void save() { Database::instance().save(Store::Speakers); }

std::string committeeOfDelegate(const std::string& id) {
    DelegateRec d;
    return delegate_find(id.c_str(), &d, nullptr) == DSA_OK ? std::string(d.committeeId) : std::string();
}
}  // namespace

SpeakerManager::SpeakerManager(ConferenceManager& conferences, AttendanceManager& attendance)
    : conf_(conferences), att_(attendance) {}

Session SpeakerManager::activeFor(const AuthContext& ctx, const std::string& sessionId, int permission) const {
    Session s = conf_.getSession(sessionId);
    RoleManager::requireCommittee(ctx, s.committeeId(), permission);
    AttendanceManager::requireActive(s);
    return s;
}

// ---------------------------------------------------------------- GSL

void SpeakerManager::addSpeaker(const AuthContext& ctx, const std::string& sessionId, const Json& body) {
    Session s = activeFor(ctx, sessionId, perm::SPEAKERS);
    std::string delegateId = requireString(body, "delegateId", DSA_ID_LEN - 1);
    if (committeeOfDelegate(delegateId) != s.committeeId())
        throw ApiError(400, "VALIDATION", delegateId + " isn't a delegate of this committee.");
    if (!att_.isSeated(sessionId, delegateId))
        throw ApiError(409, "NOT_SEATED", delegateId + " isn't marked present in this session.");
    int rc = spk_enqueue(sessionId.c_str(), delegateId.c_str());
    if (rc == DSA_ERR_DUPLICATE) throw ApiError(409, "ALREADY_LISTED", delegateId + " is already on the speakers list.");
    if (rc == DSA_ERR_FULL) throw ApiError(409, "LIST_FULL", "The speakers list is full (64).");
    if (rc != DSA_OK) throw ApiError(500, "STORE", "The speakers list couldn't be updated.");
    req_remove(sessionId.c_str(), delegateId.c_str());   // a waiting request is now satisfied
    save();
}

void SpeakerManager::removeSpeaker(const AuthContext& ctx, const std::string& sessionId,
                                   const std::string& delegateId) {
    Session s = conf_.getSession(sessionId);
    if (!(ctx.isDelegate() && ctx.accountId == delegateId))  // delegates may remove themselves
        RoleManager::requireCommittee(ctx, s.committeeId(), perm::SPEAKERS);
    if (spk_remove(sessionId.c_str(), delegateId.c_str()) != DSA_OK)
        throw ApiError(404, "NOT_FOUND", delegateId + " isn't on the speakers list.");
    save();
}

void SpeakerManager::nextSpeaker(const AuthContext& ctx, const std::string& sessionId) {
    activeFor(ctx, sessionId, perm::SPEAKERS);
    char id[DSA_ID_LEN];
    int rc = spk_next(sessionId.c_str(), nowEpoch(), id);
    save();  // the previous speech ended either way
    if (rc == DSA_ERR_NOT_FOUND) throw ApiError(409, "LIST_EMPTY", "Nobody is waiting on the speakers list.");
}

void SpeakerManager::finishSpeech(const AuthContext& ctx, const std::string& sessionId) {
    activeFor(ctx, sessionId, perm::SPEAKERS);
    if (spk_finish(sessionId.c_str(), nowEpoch()) != DSA_OK)
        throw ApiError(409, "NOBODY_SPEAKING", "Nobody is speaking.");
    save();
}

void SpeakerManager::setTime(const AuthContext& ctx, const std::string& sessionId, const Json& body) {
    activeFor(ctx, sessionId, perm::SPEAKERS);
    int seconds = requireInt(body, "seconds", 10, 600);
    spk_set_time(sessionId.c_str(), seconds);
    save();
}

void SpeakerManager::applySpeakingTime(const std::string& sessionId, int seconds) {
    if (spk_set_time(sessionId.c_str(), seconds) == DSA_OK) save();
}

void SpeakerManager::decideRequest(const AuthContext& ctx, const std::string& sessionId, bool approve) {
    activeFor(ctx, sessionId, perm::SPEAKERS);
    SpeakRequestRec r;
    if (req_pop_front(sessionId.c_str(), &r) != DSA_OK)
        throw ApiError(409, "NO_REQUESTS", "There are no speaking requests waiting.");
    if (approve) {
        int rc = spk_enqueue(sessionId.c_str(), r.delegateId);
        if (rc == DSA_ERR_FULL) {
            req_push(&r);  // put it back; nothing lost
            throw ApiError(409, "LIST_FULL", "The speakers list is full (64).");
        }
    }
    save();
}

void SpeakerManager::requestToSpeak(const AuthContext& ctx, const std::string& sessionId) {
    Session s = conf_.getSession(sessionId);
    att_.requireSeated(ctx, s);
    SpeakerState st;
    spk_state(sessionId.c_str(), &st);
    if (spk_position(sessionId.c_str(), ctx.accountId.c_str()) >= 0 || ctx.accountId == st.current)
        throw ApiError(409, "ALREADY_LISTED", "You're already on the speakers list.");
    SpeakRequestRec r{};
    pers_copy_field(r.sessionId, sessionId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.delegateId, ctx.accountId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.at, nowTimestamp().c_str(), DSA_STAMP_LEN);
    if (req_push(&r) == DSA_ERR_DUPLICATE)
        throw ApiError(409, "ALREADY_REQUESTED", "Your request is already waiting for the Executive Board.");
    save();
}

void SpeakerManager::withdrawRequest(const AuthContext& ctx, const std::string& sessionId) {
    RoleManager::requireDelegate(ctx);
    if (req_remove(sessionId.c_str(), ctx.accountId.c_str()) != DSA_OK)
        throw ApiError(404, "NOT_FOUND", "You have no waiting request.");
    save();
}

// ---------------------------------------------------------------- POIs

Json SpeakerManager::raisePoi(const AuthContext& ctx, const std::string& sessionId, const Json& body) {
    Session s = conf_.getSession(sessionId);
    att_.requireSeated(ctx, s);
    std::string target = optionalString(body, "targetId", DSA_ID_LEN - 1, "");
    if (target.empty()) {  // default: whoever has the floor
        SpeakerState st;
        spk_state(sessionId.c_str(), &st);
        target = st.current;
        if (target.empty()) throw ApiError(409, "NOBODY_SPEAKING", "Nobody has the floor to answer a POI.");
    }
    if (committeeOfDelegate(target) != s.committeeId())
        throw ApiError(400, "VALIDATION", target + " isn't a delegate of this committee.");
    char id[DSA_ID_LEN + 8];
    std::snprintf(id, sizeof id, "P%03d", poi_max_id_suffix("P") + 1);
    PointOfInformation p = PointOfInformation::create(id, sessionId, s.committeeId(), ctx.accountId, target,
                                                      body["text"].isString() ? body["text"].asString() : "");
    PoiRec rec = p.toRecord();
    if (poi_insert(&rec) != DSA_OK) throw ApiError(500, "STORE", "The POI couldn't be stored.");
    save();
    return p.toJson();
}

Json SpeakerManager::decidePoi(const AuthContext& ctx, const std::string& poiId, bool accept, const Json& body) {
    PoiRec rec;
    if (poi_find(poiId.c_str(), &rec) != DSA_OK) throw ApiError(404, "NOT_FOUND", "No POI with ID '" + poiId + "'.");
    RoleManager::requireCommittee(ctx, rec.committeeId, perm::POI);
    PointOfInformation p(rec);
    if (accept) p.accept(ctx.accountId);
    else p.reject(ctx.accountId, body["reason"].isString() ? body["reason"].asString() : "");
    PoiRec updated = p.toRecord();
    poi_update(&updated);  // leaves the pending queue
    save();
    return p.toJson();
}

Json SpeakerManager::withdrawPoi(const AuthContext& ctx, const std::string& poiId) {
    RoleManager::requireDelegate(ctx);
    PoiRec rec;
    if (poi_find(poiId.c_str(), &rec) != DSA_OK) throw ApiError(404, "NOT_FOUND", "No POI with ID '" + poiId + "'.");
    PointOfInformation p(rec);
    p.withdraw(ctx.accountId);
    PoiRec updated = p.toRecord();
    poi_update(&updated);
    save();
    return p.toJson();
}

Json SpeakerManager::myPois(const AuthContext& ctx) const {
    RoleManager::requireDelegate(ctx);
    std::vector<PoiRec> v(50);
    int n = poi_list(nullptr, ctx.accountId.c_str(), v.data(), 50);
    Json arr = Json::array();
    for (int i = 0; i < n; ++i) arr.push(PointOfInformation(v[static_cast<std::size_t>(i)]).toJson());
    return arr;
}

// ----------------------------------------------------------- live view

Json SpeakerManager::state(const std::string& sessionId, const AuthContext& ctx, bool full) const {
    SpeakerState st;
    spk_state(sessionId.c_str(), &st);
    char ids[GSL_CAPACITY][DSA_ID_LEN];
    int n = spk_queue(sessionId.c_str(), ids, GSL_CAPACITY);
    Json queue = Json::array();
    for (int i = 0; i < n; ++i) queue.push(delegateBrief(ids[i]));

    std::vector<SpeechRec> sp(6);
    int ns = speech_list(sessionId.c_str(), nullptr, sp.data(), 6);
    Json history = Json::array();
    for (int i = 0; i < ns; ++i) {
        Json j = Json::object();
        j.set("delegate", delegateBrief(sp[static_cast<std::size_t>(i)].delegateId))
            .set("seconds", sp[static_cast<std::size_t>(i)].seconds)
            .set("allotted", sp[static_cast<std::size_t>(i)].allotted);
        history.push(j);
    }

    std::vector<SpeakRequestRec> rq(GSL_CAPACITY);
    int nr = req_list(sessionId.c_str(), rq.data(), GSL_CAPACITY);
    Json requests = Json::array();
    bool myRequest = false;
    for (int i = 0; i < nr; ++i) {
        if (ctx.accountId == rq[static_cast<std::size_t>(i)].delegateId) myRequest = true;
        if (full) requests.push(delegateBrief(rq[static_cast<std::size_t>(i)].delegateId));
    }

    // POIs: EB sees the pending queue; delegates see their own and accepted ones.
    std::vector<PoiRec> pv(100);
    Json pending = Json::array(), accepted = Json::array(), mine = Json::array();
    int np = poi_pending(sessionId.c_str(), pv.data(), 100);
    if (full)
        for (int i = 0; i < np; ++i) pending.push(PointOfInformation(pv[static_cast<std::size_t>(i)]).toJson());
    int nl = poi_list(sessionId.c_str(), nullptr, pv.data(), 100);
    for (int i = 0; i < nl; ++i) {
        const PoiRec& p = pv[static_cast<std::size_t>(i)];
        if (p.status == POI_ACCEPTED && accepted.size() < 6) accepted.push(PointOfInformation(p).toJson());
        if (ctx.accountId == p.delegateId) mine.push(PointOfInformation(p).toJson());
    }

    Json out = Json::object();
    out.set("speakingSeconds", st.speakingSeconds)
        .set("current", delegateBrief(st.current))
        .set("startedAt", static_cast<double>(st.startedAt))
        .set("queue", queue)
        .set("history", history)
        .set("requests", requests)
        .set("requestCount", nr)
        .set("myRequest", myRequest)
        .set("myPosition", spk_position(sessionId.c_str(), ctx.accountId.c_str()))
        .set("poisPending", pending)
        .set("poisPendingCount", np)
        .set("poisAccepted", accepted)
        .set("myPois", mine);
    return out;
}

void registerSpeakerRoutes(Router& r, SpeakerManager& sm, Auth& auth) {
    auto ok = []() { return HttpResponse::ok(Json::object().set("done", true)); };
    r.post("/api/sessions/:id/speakers", [&sm, &auth, ok](const HttpRequest& q) {
        sm.addSpeaker(auth.require(q), q.param("id"), q.json()); return ok(); });
    r.del("/api/sessions/:id/speakers/:delegateId", [&sm, &auth, ok](const HttpRequest& q) {
        sm.removeSpeaker(auth.require(q), q.param("id"), q.param("delegateId")); return ok(); });
    r.post("/api/sessions/:id/speakers/next", [&sm, &auth, ok](const HttpRequest& q) {
        sm.nextSpeaker(auth.require(q), q.param("id")); return ok(); });
    r.post("/api/sessions/:id/speakers/finish", [&sm, &auth, ok](const HttpRequest& q) {
        sm.finishSpeech(auth.require(q), q.param("id")); return ok(); });
    r.put("/api/sessions/:id/speakers/time", [&sm, &auth, ok](const HttpRequest& q) {
        sm.setTime(auth.require(q), q.param("id"), q.json()); return ok(); });
    r.post("/api/sessions/:id/speak-requests", [&sm, &auth, ok](const HttpRequest& q) {
        sm.requestToSpeak(auth.require(q), q.param("id")); return ok(); });
    r.del("/api/sessions/:id/speak-requests", [&sm, &auth, ok](const HttpRequest& q) {
        sm.withdrawRequest(auth.require(q), q.param("id")); return ok(); });
    r.post("/api/sessions/:id/speak-requests/approve-next", [&sm, &auth, ok](const HttpRequest& q) {
        sm.decideRequest(auth.require(q), q.param("id"), true); return ok(); });
    r.post("/api/sessions/:id/speak-requests/decline-next", [&sm, &auth, ok](const HttpRequest& q) {
        sm.decideRequest(auth.require(q), q.param("id"), false); return ok(); });
    r.post("/api/sessions/:id/pois", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.raisePoi(auth.require(q), q.param("id"), q.json()), 201); });
    r.post("/api/pois/:id/accept", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.decidePoi(auth.require(q), q.param("id"), true, Json::object())); });
    r.post("/api/pois/:id/reject", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.decidePoi(auth.require(q), q.param("id"), false, q.json())); });
    r.post("/api/pois/:id/withdraw", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.withdrawPoi(auth.require(q), q.param("id"))); });
    r.get("/api/delegate/me/pois", [&sm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(sm.myPois(auth.require(q))); });
}

}  // namespace mun
