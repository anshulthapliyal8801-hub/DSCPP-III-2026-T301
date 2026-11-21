// MUNSphere — authorization matrix.
//
// Every route the router knows is listed here with who may use it. The test
// fails if a route is missing from the table, so new endpoints must be
// classified. Six identities call every route on real UNGA records from the
// demo data:
//   A admin, S UNGA scribe (view/attendance/reports only), O UNSC chair
//   (full permissions, other committee), D UNGA delegate, X UNSC delegate,
//   P not signed in.
// Anyone outside a route's allowed set must get 401 (not signed in) or 403
// (a few delegate routes answer 404 so they don't reveal that a record
// exists). Allowed identities are exercised on every GET and must get 200;
// writes are only sent by identities that must be refused, so the test never
// changes state. No response may ever be a 500.
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "API.h"
#include "AttendanceManager.h"
#include "AuditManager.h"
#include "Auth.h"
#include "CommitteeManager.h"
#include "Conference.h"
#include "Database.h"
#include "DelegateManager.h"
#include "DemoSeed.h"
#include "EBManager.h"
#include "LiveSession.h"
#include "MotionManager.h"
#include "PersistenceManager.h"
#include "RankingManager.h"
#include "ReportManager.h"
#include "ScoringManager.h"
#include "SpeakerManager.h"
#include "VotingManager.h"

using namespace mun;
namespace fs = std::filesystem;

enum : int { A = 1, S = 2, O = 4, D = 8, X = 16, PUBLIC = 32, SKIP = 64, HIDE404 = 128 };
const int AUTH = A | S | O | D | X;
const int DELEGATES = D | X;

struct Rule { int allowed; std::string path; };

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== Authorization matrix: every route, six identities ===\n");
    const std::string dir = "tests/tmp_authz";
    fs::remove_all(dir);
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
    ScoringManager scoring(conf, att);
    RankingManager rankings(conf, att, scoring);
    ReportManager reports(conf, att, scoring, rankings);
    AuditManager audit(auth);
    PersistenceManager persistence(audit);
    Router r;
    registerAuthRoutes(r, auth);
    registerModule1Routes(r, conf, auth);
    registerDelegateRoutes(r, dm, auth);
    registerCommitteeRoutes(r, cm, auth);
    registerEBRoutes(r, eb, auth);
    registerAttendanceRoutes(r, att, conf, auth);
    registerSpeakerRoutes(r, spk, auth);
    registerMotionRoutes(r, motions, auth);
    registerVotingRoutes(r, votes, auth);
    registerLiveRoutes(r, auth, conf, att, spk, motions, votes);
    registerScoringRoutes(r, scoring, auth);
    registerRankingRoutes(r, rankings, auth);
    registerReportRoutes(r, reports, auth);
    registerAuditRoutes(r, audit, auth);
    registerPersistenceRoutes(r, persistence, auth);
    audit.attach(r);
    if (seedDemo(r, auth) != 0) { std::printf("seeding failed\n"); return 1; }

    auto login = [&](const char* id, const char* pw) {
        HttpRequest q;
        q.method = "POST";
        q.path = "/api/auth/login";
        q.body = std::string(R"({"id":")") + id + R"(","password":")" + pw + R"("})";
        return Json::parse(r.dispatch(q).body)["data"]["token"].asString();
    };
    std::map<int, std::string> who = {{A, login("ADM001", "Admin2026")}, {S, login("EB003", "Board2026")},
                                      {O, login("EB004", "Board2026")}, {D, login("D001", "Delegate2026")},
                                      {X, login("D009", "Delegate2026")}, {PUBLIC, ""}};
    std::map<int, const char*> label = {{A, "admin"}, {S, "UNGA scribe"}, {O, "UNSC chair"}, {D, "UNGA delegate"},
                                        {X, "UNSC delegate"}, {PUBLIC, "signed out"}};

    // Concrete paths use UNGA records: SES002 is UNGA's live session, MS001
    // D001's published marksheet, V001/M001/P001 UNGA's vote/motion/POI.
    std::map<std::string, Rule> table = {
        {"POST /api/auth/login", {PUBLIC | SKIP, ""}},
        {"POST /api/auth/logout", {PUBLIC | SKIP, ""}},  // would end the test's own sessions
        {"GET /api/auth/me", {AUTH, ""}},
        {"POST /api/auth/change-password", {AUTH, ""}},
        {"POST /api/auth/request-reset", {PUBLIC | SKIP, ""}},
        {"GET /api/auth/history", {AUTH, ""}},
        {"GET /api/admin/accounts", {A, ""}},
        {"POST /api/admin/accounts/:id/deactivate", {A, "/api/admin/accounts/D002/deactivate"}},
        {"POST /api/admin/accounts/:id/activate", {A, "/api/admin/accounts/D002/activate"}},
        {"POST /api/admin/accounts/:id/reset-password", {A, "/api/admin/accounts/D002/reset-password"}},
        {"GET /api/admin/login-history", {A, ""}},
        {"GET /api/public/live", {PUBLIC, ""}},
        {"GET /api/overview", {A, ""}},
        {"GET /api/conferences", {AUTH, ""}},
        {"POST /api/conferences", {A, ""}},
        {"GET /api/conferences/:id", {AUTH, "/api/conferences/CONF001"}},
        {"PUT /api/conferences/:id", {A, "/api/conferences/CONF001"}},
        {"DELETE /api/conferences/:id", {A, "/api/conferences/CONF001"}},
        {"GET /api/committees", {AUTH, ""}},
        {"POST /api/committees", {A, ""}},
        {"GET /api/committees/:id", {AUTH, "/api/committees/COM001"}},
        {"PUT /api/committees/:id", {A, "/api/committees/COM001"}},
        {"DELETE /api/committees/:id", {A, "/api/committees/COM001"}},
        {"GET /api/sessions", {A | S | O, ""}},
        {"POST /api/sessions", {A, ""}},
        {"GET /api/sessions/:id", {A | S, "/api/sessions/SES002"}},
        {"PUT /api/sessions/:id", {A, "/api/sessions/SES002"}},
        {"DELETE /api/sessions/:id", {A, "/api/sessions/SES002"}},
        {"POST /api/sessions/:id/activate", {A, "/api/sessions/SES002/activate"}},
        {"POST /api/sessions/:id/deactivate", {A, "/api/sessions/SES002/deactivate"}},
        {"POST /api/sessions/:id/complete", {A, "/api/sessions/SES002/complete"}},
        {"GET /api/dsa/module1/structure", {A, ""}},
        {"GET /api/dsa/module1/search", {A, "/api/dsa/module1/search?type=committee&id=COM001"}},
        {"GET /api/dsa/module1/search-name", {A, "/api/dsa/module1/search-name?q=un"}},
        {"POST /api/auth/register", {PUBLIC | SKIP, ""}},
        {"GET /api/delegate/me", {DELEGATES, ""}},
        {"PUT /api/delegate/me", {DELEGATES, ""}},
        {"POST /api/delegate/me/registration", {DELEGATES, ""}},
        {"DELETE /api/delegate/me/registration", {DELEGATES, ""}},
        {"GET /api/delegate/me/sessions", {DELEGATES, ""}},
        {"GET /api/delegates", {A, ""}},
        {"GET /api/delegates/:id", {A, "/api/delegates/D002"}},
        {"PUT /api/delegates/:id", {A, "/api/delegates/D002"}},
        {"POST /api/delegates/:id/approve", {A, "/api/delegates/D023/approve"}},
        {"POST /api/delegates/:id/reject", {A, "/api/delegates/D023/reject"}},
        {"DELETE /api/delegates/:id", {A, "/api/delegates/D023"}},
        {"GET /api/dsa/module2/hash", {A, ""}},
        {"GET /api/dsa/module2/bst", {A, ""}},
        {"GET /api/dsa/module2/search", {A, "/api/dsa/module2/search?method=hash&key=D001"}},
        {"GET /api/committees/:id/roster", {A | S, "/api/committees/COM001/roster"}},
        {"GET /api/committee-summaries", {A, ""}},
        {"POST /api/delegates/:id/assign", {A, "/api/delegates/D002/assign"}},
        {"POST /api/delegates/:id/unassign", {A, "/api/delegates/D002/unassign"}},
        {"GET /api/eb", {A, ""}},
        {"GET /api/eb/permissions", {A, ""}},
        {"GET /api/eb/me", {S | O, ""}},
        {"POST /api/eb", {A, ""}},
        {"GET /api/eb/:id", {A, "/api/eb/EB001"}},
        {"PUT /api/eb/:id", {A, "/api/eb/EB001"}},
        {"POST /api/eb/:id/revoke", {A, "/api/eb/EB001/revoke"}},
        {"POST /api/eb/:id/restore", {A, "/api/eb/EB001/restore"}},
        {"GET /api/sessions/:id/attendance", {A | S, "/api/sessions/SES002/attendance"}},
        {"POST /api/sessions/:id/attendance", {A | S, "/api/sessions/SES002/attendance"}},
        {"POST /api/sessions/:id/attendance/all", {A | S, "/api/sessions/SES002/attendance/all"}},
        {"POST /api/sessions/:id/check-in", {D, "/api/sessions/SES002/check-in"}},
        {"GET /api/delegate/me/attendance", {DELEGATES, ""}},
        {"GET /api/committees/:id/attendance-report", {A | S, "/api/committees/COM001/attendance-report"}},
        {"POST /api/sessions/:id/speakers", {A, "/api/sessions/SES002/speakers"}},
        {"DELETE /api/sessions/:id/speakers/:delegateId", {A, "/api/sessions/SES002/speakers/D005"}},
        {"POST /api/sessions/:id/speakers/next", {A, "/api/sessions/SES002/speakers/next"}},
        {"POST /api/sessions/:id/speakers/finish", {A, "/api/sessions/SES002/speakers/finish"}},
        {"PUT /api/sessions/:id/speakers/time", {A, "/api/sessions/SES002/speakers/time"}},
        {"POST /api/sessions/:id/speak-requests", {D, "/api/sessions/SES002/speak-requests"}},
        {"DELETE /api/sessions/:id/speak-requests", {DELEGATES, "/api/sessions/SES002/speak-requests"}},
        {"POST /api/sessions/:id/speak-requests/approve-next", {A, "/api/sessions/SES002/speak-requests/approve-next"}},
        {"POST /api/sessions/:id/speak-requests/decline-next", {A, "/api/sessions/SES002/speak-requests/decline-next"}},
        {"POST /api/sessions/:id/pois", {D, "/api/sessions/SES002/pois"}},
        {"POST /api/pois/:id/accept", {A, "/api/pois/P001/accept"}},
        {"POST /api/pois/:id/reject", {A, "/api/pois/P001/reject"}},
        {"POST /api/pois/:id/withdraw", {0, "/api/pois/P001/withdraw"}},          // only its author (D002)
        {"GET /api/delegate/me/pois", {DELEGATES, ""}},
        {"GET /api/motion-types", {AUTH, ""}},
        {"POST /api/sessions/:id/motions", {A | D, "/api/sessions/SES002/motions"}},
        {"POST /api/sessions/:id/motions/next", {A, "/api/sessions/SES002/motions/next"}},
        {"POST /api/sessions/:id/motions/undo", {A, "/api/sessions/SES002/motions/undo"}},
        {"POST /api/motions/:id/withdraw", {D, "/api/motions/M001/withdraw"}},     // D001 proposed M001
        {"POST /api/motions/:id/out-of-order", {A, "/api/motions/M001/out-of-order"}},
        {"POST /api/sessions/:id/votes", {A, "/api/sessions/SES002/votes"}},
        {"GET /api/sessions/:id/votes", {A | S, "/api/sessions/SES001/votes"}},
        {"GET /api/votes/:id", {A | S | D, "/api/votes/V001"}},
        {"POST /api/votes/:id/ballot", {D, "/api/votes/V001/ballot"}},
        {"POST /api/votes/:id/record", {A, "/api/votes/V001/record"}},
        {"POST /api/votes/:id/close", {A, "/api/votes/V001/close"}},
        {"GET /api/live/current", {AUTH, ""}},
        {"GET /api/sessions/:id/live", {A | S | D, "/api/sessions/SES002/live"}},
        {"GET /api/live/overview", {A, ""}},
        {"GET /api/scoring/criteria", {AUTH, ""}},
        {"PUT /api/scoring/criteria", {A, ""}},
        {"GET /api/committees/:id/marksheets", {A | S, "/api/committees/COM001/marksheets"}},
        {"POST /api/committees/:id/marksheets", {0, "/api/committees/COM001/marksheets"}},  // UNGA EB with evaluate only
        {"POST /api/committees/:id/marksheets/publish", {A, "/api/committees/COM001/marksheets/publish"}},
        {"GET /api/marksheets", {A, ""}},
        {"GET /api/marksheets/:id", {A | S | D | HIDE404, "/api/marksheets/MS001"}},
        {"PUT /api/marksheets/:id", {0, "/api/marksheets/MS001"}},
        {"POST /api/marksheets/:id/submit", {0, "/api/marksheets/MS001/submit"}},
        {"POST /api/marksheets/:id/approve", {A, "/api/marksheets/MS001/approve"}},
        {"POST /api/marksheets/:id/return", {A, "/api/marksheets/MS001/return"}},
        {"POST /api/marksheets/:id/publish", {A, "/api/marksheets/MS001/publish"}},
        {"GET /api/delegate/me/marksheet", {DELEGATES, ""}},
        {"GET /api/awards/config", {AUTH, ""}},
        {"PUT /api/awards/config", {A, ""}},
        {"GET /api/committees/:id/rankings", {A | S, "/api/committees/COM001/rankings"}},
        {"GET /api/rankings/overall", {A, ""}},
        {"GET /api/rankings/compare", {A, ""}},
        {"GET /api/rankings/benchmark", {A, "/api/rankings/benchmark?n=50"}},
        {"GET /api/delegate/me/results", {DELEGATES, ""}},
        {"GET /api/reports", {AUTH, ""}},
        {"GET /api/reports/:key", {A | S, "/api/reports/committee?committeeId=COM001"}},
        {"GET /api/analytics/dsa", {A, ""}},
        {"GET /api/announcements", {AUTH, ""}},
        {"POST /api/announcements", {A, ""}},
        {"DELETE /api/announcements/:id", {A, "/api/announcements/AN001"}},
        {"GET /api/audit", {A, ""}},
        {"GET /api/audit/verify", {A, ""}},
        {"GET /api/audit/export", {A, ""}},
        {"GET /api/persistence/status", {A, ""}},
        {"POST /api/persistence/verify", {A, ""}},
        {"GET /api/backups", {A, ""}},
        {"POST /api/backups", {A, ""}},
        {"POST /api/backups/:id/restore", {A, "/api/backups/2000-01-01-000000/restore"}},
    };

    int pass = 0, fail = 0, requests = 0;
    auto check = [&](bool ok, const std::string& msg) {
        if (ok) pass++;
        else { fail++; std::printf("  FAIL %s\n", msg.c_str()); }
    };
    auto send = [&](const std::string& method, const std::string& path, const std::string& token, const std::string& body) {
        HttpRequest q;
        q.method = method;
        std::size_t qm = path.find('?');
        q.path = path.substr(0, qm);
        if (qm != std::string::npos) q.query = parseQueryString(path.substr(qm + 1));
        q.body = body;
        if (!token.empty()) q.headers["authorization"] = "Bearer " + token;
        requests++;
        return r.dispatch(q).status;
    };

    std::set<std::string> registered;
    for (const auto& route : r.routes()) {
        std::string key = route.first + " " + route.second;
        registered.insert(key);
        auto it = table.find(key);
        if (it == table.end()) { check(false, "route not classified: " + key); continue; }
        const Rule& rule = it->second;
        if (rule.allowed & SKIP) continue;
        std::string path = rule.path.empty() ? route.second : rule.path;
        bool isGet = route.first == "GET";
        for (int id : {A, S, O, D, X, PUBLIC}) {
            bool allowed = (rule.allowed & PUBLIC) || (rule.allowed & id);
            if (allowed && !isGet) continue;  // never perform permitted writes here
            int status = send(route.first, path, who[id], isGet ? "" : "{}");
            std::string what = key + " as " + label[id] + " -> " + std::to_string(status);
            check(status != 500, "server error: " + what);
            if (allowed) {
                check(status == 200, "should be allowed: " + what);
            } else if (id == PUBLIC) {
                check(status == 401, "should need login: " + what);
            } else {
                bool hidden = (rule.allowed & HIDE404) && (id == D || id == X) && status == 404;
                check(status == 403 || hidden, "should be refused: " + what);
            }
        }
    }
    for (const auto& kv : table)
        check(registered.count(kv.first) == 1, "table lists a route that doesn't exist: " + kv.first);

    std::printf("Robustness: odd parameters never cause a server error\n");
    const char* odd[] = {"..%2F..%2Fetc", "%00", "D%27%3B--", "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ", "-1", "%E2%9C%93"};
    for (const auto& route : r.routes()) {
        if (route.first != "GET" || route.second.find(':') == std::string::npos) continue;
        for (const char* v : odd) {
            std::string path = route.second;
            for (std::size_t p; (p = path.find(':')) != std::string::npos;) {
                std::size_t end = path.find('/', p);
                path.replace(p, end == std::string::npos ? std::string::npos : end - p, v);
            }
            int status = send("GET", path, who[A], "");
            check(status != 500 && status < 500, "server error on " + path + " -> " + std::to_string(status));
        }
    }
    int status = send("GET", "/api/rankings/benchmark?n=abc&order=random", who[A], "");
    check(status == 400, "non-numeric benchmark size refused");
    status = send("POST", "/api/conferences", who[A], "{\"name\":");
    check(status == 400, "malformed JSON refused");

    std::printf("Routes: %zu registered, %zu classified. Requests sent: %d\n", registered.size(), table.size(), requests);
    m1_store_reset(); account_store_reset(); eb_store_reset(); delegate_store_reset(); att_store_reset();
    spk_store_reset(); motion_store_reset(); vote_store_reset(); scoring_store_reset(); awards_store_reset();
    ann_store_reset(); audit_store_reset();
    fs::remove_all(dir);
    std::printf("=== %d passed, %d failed ===\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
