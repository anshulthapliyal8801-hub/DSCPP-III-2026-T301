// MUNSphere — C++ tests for Phase 4: a full committee session through the
// real API routes: attendance, speakers, POIs, motions, voting, the role
// rules for each, and persistence after a restart.
#include <cstdio>
#include <filesystem>
#include <string>

#include "API.h"
#include "AttendanceManager.h"
#include "Auth.h"
#include "CommitteeManager.h"
#include "Conference.h"
#include "Database.h"
#include "DelegateManager.h"
#include "EBManager.h"
#include "LiveSession.h"
#include "MotionManager.h"
#include "SpeakerManager.h"
#include "VotingManager.h"
#include "dsa_api.h"

using namespace mun;
namespace fs = std::filesystem;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) { g_pass++; }                                                \
        else { g_fail++; std::printf("  FAIL line %d: %s\n", __LINE__, msg); } \
    } while (0)

struct Result {
    int status;
    Json body;
    const Json& data() const { return body["data"]; }
    std::string code() const { return body["error"]["code"].isString() ? body["error"]["code"].asString() : ""; }
};
static Router* g_router;
static Result call(const std::string& m, const std::string& path, const std::string& tok = "", const std::string& body = "") {
    HttpRequest req;
    req.method = m;
    req.path = path;
    req.body = body;
    if (!tok.empty()) req.headers["authorization"] = "Bearer " + tok;
    HttpResponse res = g_router->dispatch(req);
    return {res.status, Json::parse(res.body)};
}
static std::string login(const std::string& id, const std::string& pw) {
    return call("POST", "/api/auth/login", "", R"({"id":")" + id + R"(","password":")" + pw + R"("})").data()["token"].asString();
}
static Json live(const std::string& tok) { return call("GET", "/api/sessions/SES001/live", tok).data(); }

static void resetAll() {
    m1_store_reset(); account_store_reset(); eb_store_reset(); delegate_store_reset();
    att_store_reset(); spk_store_reset(); motion_store_reset(); vote_store_reset();
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== Phase 4 tests: a live committee session ===\n");
    const std::string dir = "tests/tmp_live";
    fs::remove_all(dir);
    resetAll();
    Database::instance().open(dir);
    Auth::Config cfg;
    cfg.iterations = 1000;
    Auth auth(cfg);
    ConferenceManager conf;
    EBManager eb(auth, conf);
    DelegateManager dm(auth, conf);
    CommitteeManager cm(dm, conf);
    AttendanceManager att(conf);
    SpeakerManager spk(conf, att);
    VotingManager votes(conf, att);
    MotionManager motions(conf, att, votes, spk);
    Router router;
    registerAuthRoutes(router, auth);
    registerModule1Routes(router, conf, auth);
    registerDelegateRoutes(router, dm, auth);
    registerCommitteeRoutes(router, cm, auth);
    registerEBRoutes(router, eb, auth);
    registerAttendanceRoutes(router, att, conf, auth);
    registerSpeakerRoutes(router, spk, auth);
    registerMotionRoutes(router, motions, auth);
    registerVotingRoutes(router, votes, auth);
    registerLiveRoutes(router, auth, conf, att, spk, motions, votes);
    g_router = &router;

    // ---------------------------------------------------------- setup
    auth.createAccount(ROLE_ADMIN, "Asha Admin", "Admin12345", false);
    std::string A = login("ADM001", "Admin12345");
    call("POST", "/api/conferences", A, R"({"name":"MUN 2026","venue":"GEU","startDate":"2026-12-01","endDate":"2026-12-03"})");
    call("POST", "/api/committees", A, R"({"conferenceId":"CONF001","name":"General Assembly","abbreviation":"UNGA","capacity":10})");
    call("POST", "/api/committees", A, R"({"conferenceId":"CONF001","name":"AIPPM","abbreviation":"AIPPM","capacity":10})");
    const char* names[] = {"Rahul", "Meera", "Arjun", "Zoya", "Kiran"};
    const char* ports[] = {"India", "Brazil", "Chile", "Japan", "BJP"};
    std::string D[6];
    for (int i = 0; i < 5; ++i) {
        std::string body = std::string(R"({"name":")") + names[i] + R"( X","email":"d)" + std::to_string(i) + R"(@x.in","password":"Delegate123"})";
        D[i + 1] = call("POST", "/api/auth/register", "", body).data()["token"].asString();
        std::string id = "D00" + std::to_string(i + 1);
        std::string com = i < 4 ? "COM001" : "COM002";
        call("POST", "/api/delegate/me/registration", D[i + 1], R"({"conferenceId":"CONF001","preferences":[")" + com + R"("]})");
        call("POST", "/api/delegates/" + id + "/approve", A);
        call("POST", "/api/delegates/" + id + "/assign", A, R"({"committeeId":")" + com + R"(","portfolio":")" + ports[i] + R"("})");
    }
    auto mkEb = [&](const char* name, const char* role, const char* com) {
        Result r = call("POST", "/api/eb", A, std::string(R"({"name":")") + name + R"(","email":")" + name + R"(@x.in","ebRole":")" + role + R"(","committeeId":")" + com + R"("})");
        std::string id = r.data()["member"]["id"].asString(), temp = r.data()["temporaryPassword"].asString();
        std::string t = login(id, temp);
        call("POST", "/api/auth/change-password", t, R"({"currentPassword":")" + temp + R"(","newPassword":"Board2026x"})");
        return t;
    };
    std::string CHAIR = mkEb("Priya", "Chairperson", "COM001");
    std::string SCRIBE = mkEb("Sara", "Scribe", "COM001");
    std::string OTHER = mkEb("Dev", "Chairperson", "COM002");
    call("POST", "/api/sessions", A, R"({"committeeId":"COM001","title":"Opening","date":"2026-12-01","startTime":"10:00","endTime":"12:00"})");

    // ------------------------------------------------------ attendance
    std::printf("Attendance (Module 4)\n");
    CHECK(call("POST", "/api/sessions/SES001/attendance", SCRIBE, R"({"delegateId":"D001","status":"present"})").code() == "SESSION_NOT_STARTED",
          "no attendance before the session starts");
    call("POST", "/api/sessions/SES001/activate", CHAIR);
    CHECK(call("POST", "/api/sessions/SES001/check-in", D[1]).status == 200, "delegate checks in");
    CHECK(call("POST", "/api/sessions/SES001/check-in", D[1]).code() == "ALREADY_MARKED", "second check-in refused");
    CHECK(call("POST", "/api/sessions/SES001/speak-requests", D[1]).code() == "NOT_SEATED", "checked in is not yet seated");
    CHECK(call("POST", "/api/sessions/SES001/attendance", SCRIBE, R"({"delegateId":"D001","status":"present"})").status == 200,
          "Scribe marks present");
    Result sheet = call("POST", "/api/sessions/SES001/attendance", SCRIBE, R"({"delegateId":"D001","status":"late"})");
    CHECK(sheet.data()["tally"]["late"].asNumber() == 1 && sheet.data()["tally"]["present"].asNumber() == 0 &&
              sheet.data()["tally"]["unmarked"].asNumber() == 3,
          "re-marking updates, never duplicates");
    sheet = call("POST", "/api/sessions/SES001/attendance/all", SCRIBE, R"({"status":"present"})");
    CHECK(sheet.data()["tally"]["present"].asNumber() == 3 && sheet.data()["tally"]["late"].asNumber() == 1,
          "mark all present keeps explicit marks");
    CHECK(call("POST", "/api/sessions/SES001/attendance", SCRIBE, R"({"delegateId":"D005","status":"present"})").status == 400,
          "can't mark a delegate of another committee");
    CHECK(call("POST", "/api/sessions/SES001/attendance", OTHER, R"({"delegateId":"D001","status":"present"})").status == 403,
          "AIPPM chair can't mark UNGA attendance");
    CHECK(call("POST", "/api/sessions/SES001/check-in", D[5]).status == 403, "AIPPM delegate can't check in to UNGA");
    CHECK(call("POST", "/api/sessions/SES001/attendance", SCRIBE, R"({"delegateId":"D002","status":"sleeping"})").status == 400,
          "unknown status refused");
    Result mine = call("GET", "/api/delegate/me/attendance", D[1]);
    CHECK(mine.data()["sessionsHeld"].asNumber() == 1 && mine.data()["percentage"].asNumber() == 100, "late counts as attended");

    // -------------------------------------------------------- speakers
    std::printf("Speaker list and requests (Module 5)\n");
    CHECK(call("POST", "/api/sessions/SES001/speak-requests", D[1]).status == 200, "request to speak");
    CHECK(call("POST", "/api/sessions/SES001/speak-requests", D[1]).code() == "ALREADY_REQUESTED", "duplicate request refused");
    CHECK(live(D[1])["speakers"]["myRequest"].asBool(), "delegate sees own request waiting");
    CHECK(call("POST", "/api/sessions/SES001/speak-requests/approve-next", SCRIBE).status == 403, "Scribe can't run the list");
    CHECK(call("POST", "/api/sessions/SES001/speak-requests/approve-next", CHAIR).status == 200, "chair approves the oldest request");
    CHECK(call("POST", "/api/sessions/SES001/speak-requests", D[1]).code() == "ALREADY_LISTED", "listed delegate can't request again");
    call("POST", "/api/sessions/SES001/speakers", CHAIR, R"({"delegateId":"D002"})");
    call("POST", "/api/sessions/SES001/speakers", CHAIR, R"({"delegateId":"D003"})");
    CHECK(call("POST", "/api/sessions/SES001/speakers", CHAIR, R"({"delegateId":"D002"})").code() == "ALREADY_LISTED", "no duplicates");
    CHECK(call("POST", "/api/sessions/SES001/speakers", CHAIR, R"({"delegateId":"D005"})").status == 400, "other committee refused");
    CHECK(call("POST", "/api/sessions/SES001/speakers/next", CHAIR).status == 200, "call next speaker");
    Json L = live(CHAIR);
    CHECK(L["speakers"]["current"]["id"].asString() == "D001" && L["speakers"]["queue"].size() == 2 &&
              L["speakers"]["queue"].asArray()[0]["id"].asString() == "D002",
          "FIFO: D001 speaking, D002 next");
    CHECK(live(D[3])["speakers"]["myPosition"].asNumber() == 1, "delegate sees own place in line");
    CHECK(call("PUT", "/api/sessions/SES001/speakers/time", CHAIR, R"({"seconds":5})").status == 400, "speaking time bounds");

    std::printf("Points of Information\n");
    Result p1 = call("POST", "/api/sessions/SES001/pois", D[2], R"({"text":"Would the delegate of India clarify the funding source?"})");
    CHECK(p1.status == 201 && p1.data()["to"]["id"].asString() == "D001", "POI addressed to the current speaker");
    CHECK(call("POST", "/api/sessions/SES001/pois", D[1], R"({"text":"Self?"})").status == 400, "can't POI yourself");
    CHECK(call("POST", "/api/sessions/SES001/pois", D[5], R"({"text":"Hi"})").status == 403, "other committee can't raise POIs");
    CHECK(live(CHAIR)["speakers"]["poisPending"].size() == 1, "EB sees the pending queue");
    CHECK(live(D[3])["speakers"]["poisPending"].size() == 0, "other delegates don't see pending POIs");
    CHECK(call("POST", "/api/pois/P001/reject", CHAIR, "{}").status == 400, "rejection needs a reason");
    CHECK(call("POST", "/api/pois/P001/accept", SCRIBE).status == 403, "Scribe can't decide POIs");
    CHECK(call("POST", "/api/pois/P001/accept", OTHER).status == 403, "AIPPM chair can't decide UNGA POIs");
    CHECK(call("POST", "/api/pois/P001/accept", CHAIR).data()["status"].asString() == "accepted", "accepted");
    CHECK(call("POST", "/api/pois/P001/reject", CHAIR, R"({"reason":"x"})").code() == "INVALID_STATE", "decided once only");
    CHECK(call("GET", "/api/delegate/me/pois", D[2]).data().asArray()[0]["status"].asString() == "accepted", "delegate sees the decision");
    call("POST", "/api/sessions/SES001/pois", D[3], R"({"text":"Second question"})");
    CHECK(call("POST", "/api/pois/P002/withdraw", D[2]).status == 403, "only the author withdraws");
    CHECK(call("POST", "/api/pois/P002/withdraw", D[3]).data()["status"].asString() == "withdrawn", "author withdraws");
    call("POST", "/api/sessions/SES001/speakers/next", CHAIR);
    L = live(CHAIR);
    CHECK(L["speakers"]["current"]["id"].asString() == "D002" && L["speakers"]["history"].asArray()[0]["delegate"]["id"].asString() == "D001",
          "D001's speech logged in history");

    // --------------------------------------------------------- motions
    std::printf("Motions: precedence, withdraw, out of order, undo (Module 6)\n");
    CHECK(call("POST", "/api/sessions/SES001/motions", D[1], R"({"type":"moderated","totalMinutes":10,"speakingSeconds":60})").status == 400,
          "moderated caucus needs a topic");
    call("POST", "/api/sessions/SES001/motions", D[1], R"({"type":"moderated","topic":"Climate finance","totalMinutes":10,"speakingSeconds":60})");
    call("POST", "/api/sessions/SES001/motions", D[2], R"({"type":"unmoderated","totalMinutes":15})");
    call("POST", "/api/sessions/SES001/motions", D[3], R"({"type":"speaking_time","speakingSeconds":45})");
    call("POST", "/api/sessions/SES001/motions", D[4], R"({"type":"suspend"})");
    CHECK(call("POST", "/api/sessions/SES001/motions", D[5], R"({"type":"suspend"})").status == 403, "other committee can't move");
    Json pend = live(D[1])["motions"]["pending"];
    CHECK(pend.size() == 4 && pend.asArray()[0]["id"].asString() == "M004" && pend.asArray()[1]["id"].asString() == "M002" &&
              pend.asArray()[2]["id"].asString() == "M001" && pend.asArray()[3]["id"].asString() == "M003",
          "heap order: suspend, unmoderated, moderated, speaking time");
    CHECK(call("POST", "/api/motions/M001/withdraw", D[2]).status == 403, "only the proposer withdraws");
    CHECK(call("POST", "/api/motions/M001/withdraw", D[1]).data()["status"].asString() == "withdrawn", "proposer withdraws");
    CHECK(call("POST", "/api/motions/M004/out-of-order", CHAIR).data()["status"].asString() == "out_of_order", "ruled out of order");
    CHECK(live(CHAIR)["motions"]["pending"].asArray()[0]["id"].asString() == "M002", "M002 is now first");
    Result u = call("POST", "/api/sessions/SES001/motions/undo", CHAIR);
    CHECK(u.status == 200 && live(CHAIR)["motions"]["pending"].asArray()[0]["id"].asString() == "M004", "undo puts M004 back on top");
    CHECK(call("POST", "/api/sessions/SES001/motions/undo", CHAIR).code() == "NOTHING_TO_UNDO", "stack empty");

    std::printf("Voting (Module 7) and motion results\n");
    Result t1 = call("POST", "/api/sessions/SES001/motions/next", CHAIR);
    CHECK(t1.data()["id"].asString() == "M004" && t1.data()["status"].asString() == "voting", "highest precedence goes to a vote");
    CHECK(call("POST", "/api/sessions/SES001/motions/next", CHAIR).code() == "VOTE_OPEN", "one open vote at a time");
    std::string v1 = t1.data()["voteId"].asString();
    CHECK(call("POST", "/api/votes/" + v1 + "/ballot", D[1], R"({"choice":"abstain"})").code() == "NO_ABSTAIN", "no abstaining on procedural votes");
    CHECK(call("POST", "/api/votes/" + v1 + "/ballot", D[1], R"({"choice":"yes"})").status == 200, "vote yes");
    CHECK(call("POST", "/api/votes/" + v1 + "/ballot", D[1], R"({"choice":"no"})").code() == "ALREADY_VOTED", "duplicate vote refused");
    CHECK(call("POST", "/api/votes/" + v1 + "/ballot", D[5], R"({"choice":"no"})").status == 403, "other committee can't vote");
    call("POST", "/api/votes/" + v1 + "/ballot", D[2], R"({"choice":"no"})");
    Json dv = live(D[3])["vote"]["open"];
    CHECK(dv["ballotsCast"].asNumber() == 2 && dv["tally"].isNull() && dv["eligible"].asNumber() == 4,
          "delegates see progress, not the running tally");
    CHECK(live(CHAIR)["vote"]["open"]["tally"]["yes"].asNumber() == 1, "EB sees the running tally");
    CHECK(call("POST", "/api/sessions/SES001/motions/undo", CHAIR).code() == "CANT_UNDO", "no undo once ballots exist");
    call("POST", "/api/votes/" + v1 + "/record", CHAIR, R"({"delegateId":"D003","choice":"yes"})");
    Result corr = call("POST", "/api/votes/" + v1 + "/record", CHAIR, R"({"delegateId":"D002","choice":"yes"})");
    bool corrected = false;
    for (const Json& b : corr.data()["ballots"].asArray())
        if (b["delegate"]["id"].asString() == "D002" && b["choice"].asString() == "yes" && b["corrections"].asNumber() == 1) corrected = true;
    CHECK(corrected, "authorized correction recorded");
    CHECK(call("POST", "/api/votes/" + v1 + "/close", SCRIBE).status == 403, "Scribe can't close a vote");
    Result c1 = call("POST", "/api/votes/" + v1 + "/close", CHAIR);
    CHECK(c1.data()["outcome"].asString() == "passed" && c1.data()["tally"]["yes"].asNumber() == 3, "3-0 passes");
    CHECK(call("POST", "/api/votes/" + v1 + "/close", CHAIR).code() == "INVALID_STATE", "closed once");

    std::string v2 = call("POST", "/api/sessions/SES001/motions/next", CHAIR).data()["voteId"].asString();
    call("POST", "/api/votes/" + v2 + "/ballot", D[1], R"({"choice":"no"})");
    call("POST", "/api/votes/" + v2 + "/ballot", D[2], R"({"choice":"no"})");
    call("POST", "/api/votes/" + v2 + "/ballot", D[3], R"({"choice":"yes"})");
    CHECK(call("POST", "/api/votes/" + v2 + "/close", CHAIR).data()["outcome"].asString() == "failed", "1-2 fails");
    std::string v3 = call("POST", "/api/sessions/SES001/motions/next", CHAIR).data()["voteId"].asString();
    for (int i = 1; i <= 3; ++i) call("POST", "/api/votes/" + v3 + "/ballot", D[i], R"({"choice":"yes"})");
    call("POST", "/api/votes/" + v3 + "/close", CHAIR);
    L = live(CHAIR);
    CHECK(L["speakers"]["speakingSeconds"].asNumber() == 45, "passed speaking-time motion changes the GSL timer");
    bool m4passed = false, m2failed = false;
    for (const Json& m : L["motions"]["recent"].asArray()) {
        if (m["id"].asString() == "M004" && m["status"].asString() == "passed") m4passed = true;
        if (m["id"].asString() == "M002" && m["status"].asString() == "failed") m2failed = true;
    }
    CHECK(m4passed && m2failed, "motion results follow their votes");
    CHECK(call("POST", "/api/sessions/SES001/motions/undo", CHAIR).code() == "CANT_UNDO", "a decided motion can't be undone");

    std::printf("Substantive vote with two-thirds majority\n");
    CHECK(call("POST", "/api/sessions/SES001/votes", SCRIBE, R"({"subject":"DR 1.1"})").status == 403, "Scribe can't open votes");
    CHECK(call("POST", "/api/sessions/SES001/votes", CHAIR, R"({"subject":"DR 1.1","majority":"three_quarters"})").status == 400,
          "unknown majority refused");
    std::string v4 = call("POST", "/api/sessions/SES001/votes", CHAIR, R"({"subject":"Draft resolution 1.1","substantive":true,"majority":"two_thirds"})")
                         .data()["id"].asString();
    call("POST", "/api/votes/" + v4 + "/ballot", D[1], R"({"choice":"yes"})");
    call("POST", "/api/votes/" + v4 + "/ballot", D[2], R"({"choice":"yes"})");
    call("POST", "/api/votes/" + v4 + "/ballot", D[3], R"({"choice":"no"})");
    CHECK(call("POST", "/api/votes/" + v4 + "/ballot", D[4], R"({"choice":"abstain"})").status == 200, "abstain allowed on substantive");
    Result c4 = call("POST", "/api/votes/" + v4 + "/close", CHAIR);
    CHECK(c4.data()["outcome"].asString() == "passed" && c4.data()["percentages"]["yes"].asNumber() == 50,
          "2 of 3 voting yes meets two-thirds; percentages of all ballots");
    CHECK(call("GET", "/api/votes/" + v4, D[5]).status == 403, "other committee can't see the vote");
    CHECK(call("GET", "/api/sessions/SES001/votes", CHAIR).data().size() == 4, "vote history");

    std::printf("Admin monitor and session end\n");
    Result ov = call("GET", "/api/live/overview", A);
    CHECK(ov.data().size() == 1 && ov.data().asArray()[0]["current"]["id"].asString() == "D002", "admin sees the live floor");
    CHECK(call("GET", "/api/live/overview", CHAIR).status == 403, "overview is Admin only");
    CHECK(call("GET", "/api/sessions/SES001/live", OTHER).status == 403, "AIPPM chair can't watch UNGA");
    CHECK(call("GET", "/api/sessions/SES001/live", D[5]).status == 403, "AIPPM delegate can't watch UNGA");
    CHECK(call("GET", "/api/live/current", D[1]).data()["sessionId"].asString() == "SES001", "delegate finds the live session");
    call("POST", "/api/sessions/SES001/complete", CHAIR);
    CHECK(call("POST", "/api/sessions/SES001/speakers/next", CHAIR).code() == "SESSION_NOT_ACTIVE", "floor closes with the session");
    CHECK(call("POST", "/api/sessions/SES001/attendance", SCRIBE, R"({"delegateId":"D004","status":"absent"})").status == 200,
          "attendance can be corrected after the session");
    CHECK(call("GET", "/api/live/current", D[1]).data()["sessionId"].isNull(), "nothing live after completion");
    Result rep = call("GET", "/api/committees/COM001/attendance-report", CHAIR);
    CHECK(rep.data().size() == 4, "committee attendance report");

    std::printf("Persistence across restart\n");
    int beforeSpeeches = speech_count(), beforePois = poi_count(), beforeAtt = att_total();
    resetAll();
    Database::instance().open(dir);
    CHECK(speech_count() == beforeSpeeches && poi_count() == beforePois && att_total() == beforeAtt, "counts reloaded");
    SpeakerState st;
    spk_state("SES001", &st);
    CHECK(std::string(st.current) == "D002" && st.speakingSeconds == 45 && st.queueLength == 1, "speaker list reloaded");
    VoteRec vr;
    CHECK(vote_find(v4.c_str(), &vr) == DSA_OK && vr.outcome == 1 && vr.yes == 2, "vote result reloaded");
    MotionRec mr;
    CHECK(motion_find("M003", &mr) == DSA_OK && mr.status == MS_PASSED, "motion status reloaded");

    resetAll();
    fs::remove_all(dir);
    std::printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
