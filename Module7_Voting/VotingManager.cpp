// MUNSphere — Module7_Voting/VotingManager.cpp
#include "VotingManager.h"

#include <cstdio>

#include "Database.h"
#include "DelegateManager.h"
#include "RoleManager.h"

namespace mun {

namespace { void save() { Database::instance().save(Store::Votes); } }

VotingManager::VotingManager(ConferenceManager& conferences, AttendanceManager& attendance)
    : conf_(conferences), att_(attendance) {}

Vote VotingManager::load(const std::string& voteId) const {
    VoteRec v;
    if (vote_find(voteId.c_str(), &v) != DSA_OK) throw ApiError(404, "NOT_FOUND", "No vote with ID '" + voteId + "'.");
    return Vote(v);
}

std::string VotingManager::openProcedural(const std::string& sessionId, const std::string& committeeId,
                                          const std::string& subject, const std::string& motionId, int majority) {
    if (vote_open_for_session(sessionId.c_str(), nullptr) == DSA_OK)
        throw ApiError(409, "VOTE_OPEN", "Close the open vote before starting another.");
    VoteRec v{};
    char id[DSA_ID_LEN + 8];
    std::snprintf(id, sizeof id, "V%03d", vote_max_id_suffix("V") + 1);
    pers_copy_field(v.id, id, DSA_ID_LEN);
    pers_copy_field(v.sessionId, sessionId.c_str(), DSA_ID_LEN);
    pers_copy_field(v.committeeId, committeeId.c_str(), DSA_ID_LEN);
    pers_copy_field(v.subject, subject.c_str(), DSA_TEXT_LEN);
    pers_copy_field(v.motionId, motionId.c_str(), DSA_ID_LEN);
    v.substantive = 0;
    v.majority = majority;
    v.state = VS_OPEN;
    v.outcome = -1;
    pers_copy_field(v.openedAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
    if (vote_open(&v) != DSA_OK) throw ApiError(500, "STORE", "The vote couldn't be stored.");
    save();
    return id;
}

Json VotingManager::open(const AuthContext& ctx, const std::string& sessionId, const Json& body) {
    Session s = conf_.getSession(sessionId);
    RoleManager::requireCommittee(ctx, s.committeeId(), perm::VOTING);
    AttendanceManager::requireActive(s);
    std::string subject = requireString(body, "subject", DSA_TEXT_LEN - 1);
    bool substantive = body["substantive"].isBool() ? body["substantive"].asBool() : true;
    std::string maj = optionalString(body, "majority", 20, "simple");
    if (maj != "simple" && maj != "two_thirds")
        throw ApiError(400, "VALIDATION", "Majority must be simple or two_thirds.");
    std::string id = openProcedural(sessionId, s.committeeId(), subject, "",
                                    maj == "two_thirds" ? MAJ_TWO_THIRDS : MAJ_SIMPLE);
    VoteRec v;
    vote_find(id.c_str(), &v);
    v.substantive = substantive ? 1 : 0;
    vote_update(&v);
    save();
    return Vote(v).toJson(true);
}

bool VotingManager::discardIfEmpty(const std::string& voteId) {
    int counts[VOTE_CHOICE_COUNT];
    if (vote_tally(voteId.c_str(), counts) > 0) return false;
    vote_delete(voteId.c_str());
    save();
    return true;
}

Json VotingManager::cast(const AuthContext& ctx, const std::string& voteId, const Json& body) {
    Vote v = load(voteId);
    att_.requireSeated(ctx, conf_.getSession(v.record().sessionId));
    if (!v.open()) throw ApiError(409, "VOTE_CLOSED", "Voting has closed.");
    int choice = Vote::parseChoice(requireString(body, "choice", 10));
    v.checkChoiceAllowed(choice);
    BallotRec b{};
    pers_copy_field(b.voteId, voteId.c_str(), DSA_ID_LEN);
    pers_copy_field(b.delegateId, ctx.accountId.c_str(), DSA_ID_LEN);
    b.choice = choice;
    pers_copy_field(b.castAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
    if (ballot_cast(&b) == DSA_ERR_DUPLICATE)
        throw ApiError(409, "ALREADY_VOTED", "You've already voted. Ask the Executive Board if it needs correcting.");
    save();
    Json j = Json::object();
    j.set("vote", load(voteId).toJson(false)).set("myChoice", Vote::choiceKey(choice));
    return j;
}

Json VotingManager::record(const AuthContext& ctx, const std::string& voteId, const Json& body) {
    Vote v = load(voteId);
    RoleManager::requireCommittee(ctx, v.record().committeeId, perm::VOTING);
    if (!v.open()) throw ApiError(409, "VOTE_CLOSED", "Voting has closed.");
    std::string delegateId = requireString(body, "delegateId", DSA_ID_LEN - 1);
    int choice = Vote::parseChoice(requireString(body, "choice", 10));
    v.checkChoiceAllowed(choice);
    if (!att_.isSeated(v.record().sessionId, delegateId))
        throw ApiError(409, "NOT_SEATED", delegateId + " isn't marked present, so can't vote.");
    DelegateRec d;
    if (delegate_find(delegateId.c_str(), &d, nullptr) != DSA_OK || v.record().committeeId != std::string(d.committeeId))
        throw ApiError(400, "VALIDATION", delegateId + " isn't a delegate of this committee.");
    BallotRec existing;
    if (ballot_find(voteId.c_str(), delegateId.c_str(), &existing, nullptr) == DSA_OK) {
        ballot_correct(voteId.c_str(), delegateId.c_str(), choice);  // authorized correction
    } else {
        BallotRec b{};
        pers_copy_field(b.voteId, voteId.c_str(), DSA_ID_LEN);
        pers_copy_field(b.delegateId, delegateId.c_str(), DSA_ID_LEN);
        b.choice = choice;
        pers_copy_field(b.castAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
        ballot_cast(&b);
    }
    save();
    return details(ctx, voteId);
}

Json VotingManager::close(const AuthContext& ctx, const std::string& voteId) {
    Vote v = load(voteId);
    RoleManager::requireCommittee(ctx, v.record().committeeId, perm::VOTING);
    int counts[VOTE_CHOICE_COUNT];
    vote_tally(voteId.c_str(), counts);  // counted in C from the ballot array
    v.close(counts[VOTE_YES], counts[VOTE_NO], counts[VOTE_ABSTAIN]);
    VoteRec rec = v.record();
    vote_update(&rec);
    save();
    for (const CloseHook& h : hooks_) h(rec);  // e.g. motions apply the result
    return v.toJson(true);
}

Json VotingManager::details(const AuthContext& ctx, const std::string& voteId) const {
    Vote v = load(voteId);
    bool full = RoleManager::canCommittee(ctx, v.record().committeeId, perm::VIEW);
    if (!full) {
        DelegateRec d;
        if (!ctx.isDelegate() || delegate_find(ctx.accountId.c_str(), &d, nullptr) != DSA_OK ||
            v.record().committeeId != std::string(d.committeeId))
            throw ApiError(403, "FORBIDDEN", "This vote belongs to another committee.");
    }
    Json j = v.toJson(full);
    if (full) {
        BallotRec b[512];
        int n = ballot_list(voteId.c_str(), b, 512);
        Json ballots = Json::array();
        for (int i = 0; i < n; ++i) {
            Json x = Json::object();
            x.set("delegate", delegateBrief(b[i].delegateId)).set("choice", Vote::choiceKey(b[i].choice))
                .set("corrections", b[i].corrections);
            ballots.push(x);
        }
        j.set("ballots", ballots);
    }
    return j;
}

Json VotingManager::history(const AuthContext& ctx, const std::string& sessionId) const {
    Session s = conf_.getSession(sessionId);
    RoleManager::requireCommittee(ctx, s.committeeId(), perm::VIEW);
    VoteRec v[100];
    int n = vote_list(sessionId.c_str(), v, 100);
    Json arr = Json::array();
    for (int i = 0; i < n; ++i) arr.push(Vote(v[i]).toJson(true));
    return arr;
}

Json VotingManager::state(const std::string& sessionId, const AuthContext& ctx, bool full) const {
    Json out = Json::object();
    VoteRec open;
    if (vote_open_for_session(sessionId.c_str(), &open) == DSA_OK) {
        Json j = Vote(open).toJson(full);
        BallotRec mine;
        if (ballot_find(open.id, ctx.accountId.c_str(), &mine, nullptr) == DSA_OK)
            j.set("myChoice", Vote::choiceKey(mine.choice));
        int eligible = 0;
        Session s = conf_.getSession(sessionId);
        for (const DelegateRec& d : delegatesInCommittee(s.committeeId()))
            if (att_.isSeated(sessionId, d.id)) eligible++;
        j.set("eligible", eligible);
        out.set("open", j);
    } else {
        out.set("open", Json());
    }
    VoteRec recent[4];
    int n = vote_list(sessionId.c_str(), recent, 4);
    Json closed = Json::array();
    for (int i = 0; i < n; ++i)
        if (recent[i].state == VS_CLOSED) closed.push(Vote(recent[i]).toJson(true));
    out.set("recent", closed);
    return out;
}

void registerVotingRoutes(Router& r, VotingManager& vm, Auth& auth) {
    r.post("/api/sessions/:id/votes", [&vm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(vm.open(auth.require(q), q.param("id"), q.json()), 201); });
    r.get("/api/sessions/:id/votes", [&vm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(vm.history(auth.require(q), q.param("id"))); });
    r.get("/api/votes/:id", [&vm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(vm.details(auth.require(q), q.param("id"))); });
    r.post("/api/votes/:id/ballot", [&vm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(vm.cast(auth.require(q), q.param("id"), q.json())); });
    r.post("/api/votes/:id/record", [&vm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(vm.record(auth.require(q), q.param("id"), q.json())); });
    r.post("/api/votes/:id/close", [&vm, &auth](const HttpRequest& q) {
        return HttpResponse::ok(vm.close(auth.require(q), q.param("id"))); });
}

}  // namespace mun
