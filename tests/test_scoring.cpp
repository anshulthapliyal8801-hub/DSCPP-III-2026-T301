// MUNSphere — C++ tests for Phase 5: marksheets, rankings, awards, sorting,
// through the real routes.
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
#include "RankingManager.h"
#include "ScoringManager.h"
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
    std::size_t q = path.find('?');
    req.path = path.substr(0, q);
    if (q != std::string::npos) req.query = parseQueryString(path.substr(q + 1));
    req.body = body;
    if (!tok.empty()) req.headers["authorization"] = "Bearer " + tok;
    HttpResponse res = g_router->dispatch(req);
    return {res.status, Json::parse(res.body)};
}
static std::string login(const std::string& id, const std::string& pw) {
    return call("POST", "/api/auth/login", "", R"({"id":")" + id + R"(","password":")" + pw + R"("})").data()["token"].asString();
}
static void resetAll() {
    m1_store_reset(); account_store_reset(); eb_store_reset(); delegate_store_reset();
    att_store_reset(); scoring_store_reset(); awards_store_reset();
}
static std::string marks(int r, int s, int p, int d, int pm, int a) {
    char buf[200];
    std::snprintf(buf, sizeof buf, R"({"marks":{"research":%d,"speaking":%d,"participation":%d,"diplomacy":%d,"poi_motion":%d,"attendance":%d}})",
                  r, s, p, d, pm, a);
    return buf;
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== Phase 5 tests: marksheets, rankings, awards ===\n");
    const std::string dir = "tests/tmp_scoring";
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
    ScoringManager scoring(conf, att);
    RankingManager rankings(conf, att, scoring);
    Router router;
    registerAuthRoutes(router, auth);
    registerModule1Routes(router, conf, auth);
    registerDelegateRoutes(router, dm, auth);
    registerCommitteeRoutes(router, cm, auth);
    registerEBRoutes(router, eb, auth);
    registerAttendanceRoutes(router, att, conf, auth);
    registerScoringRoutes(router, scoring, auth);
    registerRankingRoutes(router, rankings, auth);
    g_router = &router;

    // ------------------------------------------------------------ setup
    auth.createAccount(ROLE_ADMIN, "Asha Admin", "Admin12345", false);
    std::string A = login("ADM001", "Admin12345");
    call("POST", "/api/conferences", A, R"({"name":"MUN","venue":"GEU","startDate":"2026-12-01","endDate":"2026-12-03"})");
    call("POST", "/api/committees", A, R"({"conferenceId":"CONF001","name":"General Assembly","abbreviation":"UNGA","capacity":10})");
    call("POST", "/api/committees", A, R"({"conferenceId":"CONF001","name":"AIPPM","abbreviation":"AIPPM","capacity":10})");
    std::string D[6];
    const char* ports[] = {"India", "Brazil", "Chile", "Japan", "BJP"};
    for (int i = 0; i < 5; ++i) {
        std::string body = R"({"name":"Delegate )" + std::to_string(i + 1) + R"(","email":"d)" + std::to_string(i) + R"(@x.in","password":"Delegate123"})";
        D[i + 1] = call("POST", "/api/auth/register", "", body).data()["token"].asString();
        std::string id = "D00" + std::to_string(i + 1), com = i < 4 ? "COM001" : "COM002";
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
    std::string MOD = mkEb("Kabir", "Moderator", "COM001");
    std::string OTHER = mkEb("Dev", "Chairperson", "COM002");
    // Two sessions: D003 absent in the first, so attendance differs.
    call("POST", "/api/sessions", A, R"({"committeeId":"COM001","title":"S1","date":"2026-12-01","startTime":"10:00","endTime":"11:00"})");
    call("POST", "/api/sessions", A, R"({"committeeId":"COM001","title":"S2","date":"2026-12-01","startTime":"12:00","endTime":"13:00"})");
    call("POST", "/api/sessions/SES001/activate", A);
    call("POST", "/api/sessions/SES001/attendance", A, R"({"delegateId":"D002","status":"late"})");
    call("POST", "/api/sessions/SES001/attendance", A, R"({"delegateId":"D003","status":"absent"})");
    call("POST", "/api/sessions/SES001/attendance/all", A, R"({"status":"present"})");
    call("POST", "/api/sessions/SES001/complete", A);
    call("POST", "/api/sessions/SES002/activate", A);
    call("POST", "/api/sessions/SES002/attendance/all", A, R"({"status":"present"})");
    call("POST", "/api/sessions/SES002/complete", A);

    // --------------------------------------------------------- criteria
    std::printf("Scoring criteria\n");
    Result cr = call("GET", "/api/scoring/criteria", D[1]);
    CHECK(cr.data()["criteria"].size() == 6 && cr.data()["total"].asNumber() == 100, "default criteria from the spec, total 100");
    CHECK(call("PUT", "/api/scoring/criteria", CHAIR, R"({"criteria":[{"name":"X","maxMarks":10}]})").status == 403, "only Admin configures");
    CHECK(call("PUT", "/api/scoring/criteria", A, R"({"criteria":[{"name":"A","maxMarks":10,"fromAttendance":true},{"name":"B","maxMarks":10,"fromAttendance":true}]})").status == 400,
          "one attendance criterion at most");
    CHECK(call("PUT", "/api/scoring/criteria", A, R"({"criteria":[{"name":"Same","maxMarks":10},{"name":"same","maxMarks":10}]})").status == 400,
          "duplicate names refused");
    CHECK(call("PUT", "/api/scoring/criteria", A, R"({"criteria":[{"name":"A","maxMarks":0}]})").status == 400, "max 0 refused");

    // ------------------------------------------------- marksheet workflow
    std::printf("Marksheet workflow and separation of duties\n");
    CHECK(call("POST", "/api/committees/COM001/marksheets", MOD, R"({"delegateId":"D001"})").status == 403, "Moderator lacks evaluate");
    CHECK(call("POST", "/api/committees/COM001/marksheets", A, R"({"delegateId":"D001"})").status == 403, "Admin doesn't write marks");
    CHECK(call("POST", "/api/committees/COM001/marksheets", OTHER, R"({"delegateId":"D001"})").status == 403, "other committee's chair refused");
    CHECK(call("POST", "/api/committees/COM001/marksheets", CHAIR, R"({"delegateId":"D005"})").status == 400, "delegate must be in the committee");
    Result c1 = call("POST", "/api/committees/COM001/marksheets", CHAIR, R"({"delegateId":"D001"})");
    CHECK(c1.status == 201 && c1.data()["status"].asString() == "draft", "chair creates a draft");
    CHECK(c1.data()["criteria"].asArray()[5]["mark"].asNumber() == 10, "attendance prefilled: 2 of 2 sessions = 10/10");
    std::string ms1 = c1.data()["id"].asString();
    Result c3 = call("POST", "/api/committees/COM001/marksheets", CHAIR, R"({"delegateId":"D003"})");
    CHECK(c3.data()["criteria"].asArray()[5]["mark"].asNumber() == 5, "D003 attended 1 of 2: 5/10");
    CHECK(call("POST", "/api/committees/COM001/marksheets", CHAIR, R"({"delegateId":"D001"})").code() == "DUPLICATE", "one marksheet per delegate");
    Result bad = call("PUT", "/api/marksheets/" + ms1, CHAIR, R"({"marks":{"research":25}})");
    CHECK(bad.status == 400 && bad.body["error"]["message"].asString().find("Research") != std::string::npos, "mark above max names the criterion");
    CHECK(call("PUT", "/api/marksheets/" + ms1, CHAIR, R"({"marks":{"charisma":5}})").status == 400, "unknown criterion refused");
    CHECK(call("PUT", "/api/marksheets/" + ms1, CHAIR, R"({"marks":{"research":7.5}})").status == 400, "whole marks only");
    Result sv = call("PUT", "/api/marksheets/" + ms1, CHAIR, marks(18, 17, 16, 12, 7, 10));
    CHECK(sv.data()["total"].asNumber() == 80 && sv.data()["percent"].asNumber() == 80, "total computed in C: 80");
    call("PUT", "/api/marksheets/" + ms1, CHAIR, R"({"remarks":"Strong research, work on rebuttals."})");
    CHECK(call("PUT", "/api/marksheets/" + ms1, D[1], marks(20, 20, 20, 15, 15, 10)).status == 403, "delegate can't change marks");
    CHECK(call("GET", "/api/marksheets/" + ms1, D[1]).status == 404, "delegate can't see a draft");
    CHECK(call("GET", "/api/delegate/me/marksheet", D[1]).data().isNull(), "no published sheet yet");

    CHECK(call("POST", "/api/marksheets/" + ms1 + "/approve", A).code() == "INVALID_STATE", "can't approve a draft");
    CHECK(call("POST", "/api/marksheets/" + ms1 + "/submit", CHAIR).data()["status"].asString() == "submitted", "submit");
    CHECK(call("PUT", "/api/marksheets/" + ms1, CHAIR, marks(20, 20, 20, 15, 15, 10)).code() == "INVALID_STATE", "no edits after submission");
    CHECK(call("POST", "/api/marksheets/" + ms1 + "/approve", CHAIR).status == 403, "EB can't approve their own marksheet");
    CHECK(call("POST", "/api/marksheets/" + ms1 + "/approve", A).data()["status"].asString() == "approved", "Admin approves");
    CHECK(call("POST", "/api/marksheets/" + ms1 + "/return", A, "{}").status == 400, "returning needs a note");
    call("POST", "/api/marksheets/" + ms1 + "/return", A, R"({"note":"Diplomacy looks high for two speeches."})");
    Result back = call("GET", "/api/marksheets/" + ms1, CHAIR);
    CHECK(back.data()["status"].asString() == "returned" && back.data()["adminNote"].asString().find("Diplomacy") == 0, "EB sees why it came back");
    call("PUT", "/api/marksheets/" + ms1, CHAIR, marks(18, 17, 16, 12, 7, 10));
    call("POST", "/api/marksheets/" + ms1 + "/submit", CHAIR);
    call("POST", "/api/marksheets/" + ms1 + "/approve", A);
    CHECK(call("POST", "/api/marksheets/" + ms1 + "/publish", CHAIR).status == 403, "EB can't publish");
    Result pub = call("POST", "/api/marksheets/" + ms1 + "/publish", A);
    CHECK(pub.data()["status"].asString() == "published", "Admin publishes");
    std::string hist;
    for (const Json& h : pub.data()["history"].asArray()) hist += h["action"].asString() + ",";
    CHECK(hist == "created,saved,saved,submitted,approved,returned,saved,submitted,approved,published,", "full revision history");
    Result own = call("GET", "/api/marksheets/" + ms1, D[1]);
    CHECK(own.status == 200 && own.data()["adminNote"].isNull() && own.data()["history"].isNull(),
          "delegate sees own published sheet without internal notes");
    CHECK(call("GET", "/api/marksheets/" + ms1, D[2]).status == 404, "other delegates can't see it");

    // ------------------------------------------------- rankings and ties
    std::printf("Rankings, ties and awards\n");
    std::string ms3 = c3.data()["id"].asString();
    std::string ms2 = call("POST", "/api/committees/COM001/marksheets", CHAIR, R"({"delegateId":"D002"})").data()["id"].asString();
    std::string ms4 = call("POST", "/api/committees/COM001/marksheets", CHAIR, R"({"delegateId":"D004"})").data()["id"].asString();
    call("PUT", "/api/marksheets/" + ms2, CHAIR, marks(15, 18, 14, 10, 5, 10));   // 72, speaking 18, attendance 100
    call("PUT", "/api/marksheets/" + ms4, CHAIR, marks(14, 18, 15, 10, 5, 10));   // 72, speaking 18, attendance 100
    call("PUT", "/api/marksheets/" + ms3, CHAIR, marks(16, 15, 16, 10, 10, 5));   // 72, speaking 15, attendance 50
    for (const std::string& id : {ms2, ms3, ms4}) {
        call("POST", "/api/marksheets/" + id + "/submit", CHAIR);
        call("POST", "/api/marksheets/" + id + "/approve", A);
    }
    Result rk = call("GET", "/api/committees/COM001/rankings?algorithm=bubble", A);
    const Json::Array& rows = rk.data()["rows"].asArray();
    CHECK(rows.size() == 4 && rows[0]["delegate"]["id"].asString() == "D001" && rows[0]["rank"].asNumber() == 1, "D001 first");
    CHECK(rows[1]["rank"].asNumber() == 2 && rows[2]["rank"].asNumber() == 2 && rows[1]["tied"].asBool() &&
              rows[1]["delegate"]["id"].asString() == "D002" && rows[2]["delegate"]["id"].asString() == "D004",
          "genuine tie shares rank 2");
    CHECK(rows[3]["delegate"]["id"].asString() == "D003" && rows[3]["rank"].asNumber() == 4 &&
              rows[3]["separatedBy"].asString() == "tie-break criterion",
          "competition ranking skips to 4; tie broken by the speaking criterion");
    CHECK(rows[0]["award"].asString() == "Best Delegate" && rows[1]["award"].asString() == "High Commendation" &&
              rows[2]["award"].asString() == "High Commendation" && rows[3]["award"].asString() == "Special Mention",
          "awards follow rank; a tie shares the award");
    CHECK(rk.data()["sort"]["name"].asString() == "Bubble sort" && rk.data()["sort"]["comparisons"].asNumber() > 0,
          "the ranking reports its real sort work");
    Result cmpAll = call("GET", "/api/rankings/compare?committeeId=COM001", A);
    CHECK(cmpAll.data()["identicalRankings"].asBool() && cmpAll.data()["results"].size() == 5, "all five algorithms agree");

    CHECK(call("GET", "/api/committees/COM001/rankings", CHAIR).data()["scope"].asString() == "all", "EB sees provisional ranking of own committee");
    CHECK(call("GET", "/api/committees/COM001/rankings", OTHER).status == 403, "other committee's EB refused");
    CHECK(call("GET", "/api/committees/COM001/rankings", D[1]).status == 403, "delegates use their results page");
    CHECK(call("GET", "/api/rankings/overall", A).data()["rows"].size() == 4, "overall ranking across committees");

    Result r1 = call("GET", "/api/delegate/me/results", D[1]);
    CHECK(r1.data()["rank"].asNumber() == 1 && r1.data()["of"].asNumber() == 1 && r1.data()["award"].asString() == "Best Delegate",
          "delegate ranked among published results only");
    CHECK(call("GET", "/api/delegate/me/results", D[2]).data()["marksheet"].isNull(), "unpublished: nothing shown");
    CHECK(call("POST", "/api/committees/COM001/marksheets/publish", A).data()["published"].asNumber() == 3, "publish the rest");
    Result r2 = call("GET", "/api/delegate/me/results", D[2]);
    CHECK(r2.data()["rank"].asNumber() == 2 && r2.data()["of"].asNumber() == 4 && r2.data()["award"].asString() == "High Commendation" &&
              r2.data()["committeeAwards"].size() == 4,
          "after publication: rank, award and the committee's awards");

    std::printf("Configurable thresholds and tie-break\n");
    CHECK(call("PUT", "/api/awards/config", A, R"({"rules":[{"name":"Best","positions":1,"minPercent":70}],"tiebreakKey":"charisma"})").status == 400,
          "tie-break must be a criterion");
    CHECK(call("PUT", "/api/awards/config", CHAIR, R"({"rules":[{"name":"Best","positions":1,"minPercent":70}]})").status == 403, "Admin only");
    call("PUT", "/api/awards/config", A, R"({"rules":[{"name":"Best Delegate","positions":1,"minPercent":85},{"name":"Commendation","positions":3,"minPercent":70}],"tiebreakKey":"research"})");
    rk = call("GET", "/api/committees/COM001/rankings?algorithm=quick", A);
    const Json::Array& rows2 = rk.data()["rows"].asArray();
    CHECK(rows2[1]["delegate"]["id"].asString() == "D003" && rows2[2]["delegate"]["id"].asString() == "D002" &&
              rows2[3]["delegate"]["id"].asString() == "D004" && rows2[3]["rank"].asNumber() == 4,
          "research as tie-break reorders the 72% group (16, 15, 14)");
    CHECK(rows2[0]["award"].asString() == "Commendation" && rows2[1]["award"].asString() == "Commendation" &&
              rows2[3]["award"].isNull(),
          "80% misses the 85% threshold; the next award takes its 3 places");

    std::printf("Sorting benchmark on generated data\n");
    Result bs = call("GET", "/api/rankings/benchmark?n=1000&order=sorted", A);
    const Json::Array& b = bs.data()["results"].asArray();
    CHECK(b[0]["comparisons"].asNumber() == 999 && b[2]["comparisons"].asNumber() == 999, "bubble and insertion: n-1 on sorted");
    CHECK(b[1]["comparisons"].asNumber() == 499500 && b[4]["comparisons"].asNumber() == 499500, "selection and quick: n(n-1)/2 on sorted");
    CHECK(bs.data()["data"].asString().find("Generated") == 0, "labelled as generated data");
    Result br = call("GET", "/api/rankings/benchmark?n=2000&order=random", A);
    CHECK(br.data()["results"].asArray()[3]["comparisons"].asNumber() < br.data()["results"].asArray()[0]["comparisons"].asNumber() / 20,
          "merge does far fewer comparisons than bubble on random data");
    CHECK(call("GET", "/api/rankings/benchmark?n=99999", A).status == 400, "size limit");
    CHECK(call("GET", "/api/rankings/benchmark?order=shuffled", A).status == 400, "unknown order");

    std::printf("Criteria snapshots\n");
    call("PUT", "/api/scoring/criteria", A, R"({"criteria":[{"name":"Research","maxMarks":25},{"name":"Speaking","maxMarks":25},{"name":"Participation","maxMarks":20},{"name":"POI and Motion Quality","maxMarks":20},{"name":"Attendance","maxMarks":10,"fromAttendance":true}]})");
    Result c5 = call("POST", "/api/committees/COM002/marksheets", OTHER, R"({"delegateId":"D005"})");
    CHECK(c5.data()["criteria"].size() == 5 && c5.data()["maxTotal"].asNumber() == 100, "new marksheet uses the new criteria");
    CHECK(call("GET", "/api/marksheets/" + ms1, A).data()["criteria"].size() == 6, "existing marksheet keeps its snapshot");

    std::printf("Persistence across restart\n");
    int sheets = sheet_count(), revs = rev_count();
    resetAll();
    Database::instance().open(dir);
    CHECK(sheet_count() == sheets && rev_count() == revs && criteria_count() == 5 && awards_count() == 2, "everything reloaded");
    MarksheetRec m;
    CHECK(sheet_find(ms1.c_str(), &m, nullptr) == DSA_OK && m.status == SHEET_PUBLISHED && m.total == 80 && m.criteriaCount == 6,
          "published sheet with its snapshot reloaded");

    resetAll();
    fs::remove_all(dir);
    std::printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
