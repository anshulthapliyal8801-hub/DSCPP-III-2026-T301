// MUNSphere — C++ tests for Phase 6: audit log, reports and exports,
// analytics, announcements, backup/restore, round trip and integrity.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "API.h"
#include "AttendanceManager.h"
#include "AuditManager.h"
#include "Auth.h"
#include "CommitteeManager.h"
#include "Conference.h"
#include "Database.h"
#include "DelegateManager.h"
#include "EBManager.h"
#include "MotionManager.h"
#include "PersistenceManager.h"
#include "RankingManager.h"
#include "ReportManager.h"
#include "ScoringManager.h"
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
    std::string raw, type, disposition;
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
    Result r{res.status, Json(), res.body, res.contentType, ""};
    for (const auto& h : res.headers) if (h.first == "Content-Disposition") r.disposition = h.second;
    if (res.contentType.rfind("application/json", 0) == 0) r.body = Json::parse(res.body);
    return r;
}
static std::string login(const std::string& id, const std::string& pw) {
    return call("POST", "/api/auth/login", "", R"({"id":")" + id + R"(","password":")" + pw + R"("})").data()["token"].asString();
}
static void resetAll() {
    m1_store_reset(); account_store_reset(); eb_store_reset(); delegate_store_reset(); att_store_reset();
    spk_store_reset(); motion_store_reset(); vote_store_reset(); scoring_store_reset(); awards_store_reset();
    ann_store_reset(); audit_store_reset();
}
static std::string readFile(const std::string& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== Phase 6 tests: audit, reports, backup ===\n");
    const std::string dir = "tests/tmp_reports";
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
    ScoringManager scoring(conf, att);
    RankingManager rankings(conf, att, scoring);
    ReportManager reports(conf, att, scoring, rankings);
    AuditManager audit(auth);
    PersistenceManager persistence(audit);
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
    registerScoringRoutes(router, scoring, auth);
    registerRankingRoutes(router, rankings, auth);
    registerReportRoutes(router, reports, auth);
    registerAuditRoutes(router, audit, auth);
    registerPersistenceRoutes(router, persistence, auth);
    audit.attach(router);
    g_router = &router;

    // ------------------------------------------------------------ setup
    auth.createAccount(ROLE_ADMIN, "Asha Admin", "Admin12345", false);
    std::string A = login("ADM001", "Admin12345");
    call("POST", "/api/conferences", A, R"({"name":"MUN 2026","venue":"GEU","startDate":"2026-12-01","endDate":"2026-12-03"})");
    call("POST", "/api/committees", A, R"({"conferenceId":"CONF001","name":"General Assembly","abbreviation":"UNGA","capacity":10})");
    call("POST", "/api/committees", A, R"({"conferenceId":"CONF001","name":"AIPPM","abbreviation":"AIPPM","capacity":10})");
    const char* names[] = {R"(Sharma, \"Rahul\")", "Meera Iyer", "Arjun Rao"};
    const char* ports[] = {"India", "Brazil", "Chile"};
    std::string D[4];
    for (int i = 0; i < 3; ++i) {
        std::string body = std::string(R"({"name":")") + names[i] + R"(","email":"d)" + std::to_string(i) + R"(@x.in","password":"Delegate123"})";
        D[i + 1] = call("POST", "/api/auth/register", "", body).data()["token"].asString();
        std::string id = "D00" + std::to_string(i + 1);
        call("POST", "/api/delegate/me/registration", D[i + 1], R"({"conferenceId":"CONF001","preferences":["COM001"]})");
        call("POST", "/api/delegates/" + id + "/approve", A);
        call("POST", "/api/delegates/" + id + "/assign", A, R"({"committeeId":"COM001","portfolio":")" + std::string(ports[i]) + R"("})");
    }
    auto mkEb = [&](const char* name, const char* role) {
        Result r = call("POST", "/api/eb", A, std::string(R"({"name":")") + name + R"(","email":")" + name + R"(@x.in","ebRole":")" + role + R"(","committeeId":"COM001"})");
        std::string id = r.data()["member"]["id"].asString(), temp = r.data()["temporaryPassword"].asString();
        std::string t = login(id, temp);
        call("POST", "/api/auth/change-password", t, R"({"currentPassword":")" + temp + R"(","newPassword":"Board2026x"})");
        return t;
    };
    std::string CHAIR = mkEb("Priya", "Chairperson"), SCRIBE = mkEb("Sara", "Scribe"), MOD = mkEb("Kabir", "Moderator");
    call("POST", "/api/sessions", A, R"({"committeeId":"COM001","title":"Opening","date":"2026-12-01","startTime":"10:00","endTime":"12:00"})");
    call("POST", "/api/sessions/SES001/activate", CHAIR);
    call("POST", "/api/sessions/SES001/attendance/all", CHAIR, R"({"status":"present"})");
    call("POST", "/api/sessions/SES001/speakers", CHAIR, R"({"delegateId":"D001"})");
    call("POST", "/api/sessions/SES001/speakers/next", CHAIR);
    call("POST", "/api/sessions/SES001/pois", D[2], R"({"text":"A question"})");
    call("POST", "/api/pois/P001/accept", CHAIR);
    call("POST", "/api/sessions/SES001/speakers/finish", CHAIR);
    call("POST", "/api/sessions/SES001/motions", D[2], R"({"type":"suspend"})");
    std::string v1 = call("POST", "/api/sessions/SES001/motions/next", CHAIR).data()["voteId"].asString();
    for (int i = 1; i <= 3; ++i) call("POST", "/api/votes/" + v1 + "/ballot", D[i], R"({"choice":"yes"})");
    call("POST", "/api/votes/" + v1 + "/close", CHAIR);
    for (const char* d : {"D001", "D002"}) {
        std::string id = call("POST", "/api/committees/COM001/marksheets", CHAIR, std::string(R"({"delegateId":")") + d + R"("})").data()["id"].asString();
        call("PUT", "/api/marksheets/" + id, CHAIR, std::string(R"({"marks":{"research":)") + (d[3] == '1' ? "18" : "12") + R"(,"speaking":15,"participation":15,"diplomacy":10,"poi_motion":10}})");
        call("POST", "/api/marksheets/" + id + "/submit", CHAIR);
        call("POST", "/api/marksheets/" + id + "/approve", A);
    }

    // ------------------------------------------------------------ audit
    std::printf("Audit log\n");
    Result al = call("GET", "/api/audit?actor=ADM001&q=created%20conference", A);
    CHECK(al.data()["entries"].size() == 1 && al.data()["entries"].asArray()[0]["target"].asString() == "new=CONF001",
          "conference creation recorded with the new ID");
    Result reg = call("GET", "/api/audit?q=delegate%20registered", A);
    CHECK(reg.data()["entries"].size() == 3 && reg.data()["entries"].asArray()[2]["actor"].asString() == "D001",
          "self-registration recorded under the new delegate's ID");
    Result ballots = call("GET", "/api/audit?q=cast%20ballot", A);
    CHECK(ballots.data()["entries"].size() == 3 && ballots.data()["entries"].asArray()[0]["role"].asString() == "delegate",
          "ballots recorded (who voted, not how)");
    int before = audit_count();
    call("GET", "/api/conferences", A);
    call("GET", "/api/sessions", CHAIR);
    CHECK(audit_count() == before, "reads aren't logged");
    call("POST", "/api/conferences", CHAIR, R"({"name":"X"})");
    Result denied = call("GET", "/api/audit?result=failed&actor=EB001", A);
    CHECK(denied.data()["entries"].size() >= 1 && denied.data()["entries"].asArray()[0]["status"].asNumber() == 403,
          "refused attempts are logged with their status");
    std::string log = readFile(dir + "/audit_log.txt");
    CHECK(log.find("Delegate123") == std::string::npos && log.find("Board2026x") == std::string::npos &&
              log.find("\"yes\"") == std::string::npos,
          "no passwords or ballot choices in the log file");
    CHECK(call("GET", "/api/audit/verify", A).data()["intact"].asBool(), "hash chain intact");
    CHECK(call("GET", "/api/audit", CHAIR).status == 403, "audit log is Admin only");
    Result ex = call("GET", "/api/audit/export", A);
    CHECK(ex.type.rfind("text/csv", 0) == 0 && ex.raw.rfind("seq,at,actor", 0) == 0, "audit CSV export");

    // ---------------------------------------------------------- reports
    std::printf("Reports and exports\n");
    CHECK(call("GET", "/api/reports", A).data().size() == 10, "Admin sees all ten reports");
    CHECK(call("GET", "/api/reports", SCRIBE).data().size() == 5, "EB sees the five committee reports");
    CHECK(call("GET", "/api/reports", D[1]).data().size() == 0, "delegates see none");
    CHECK(call("GET", "/api/reports/scoring", SCRIBE).status == 403, "EB can't run Admin-only reports");
    CHECK(call("GET", "/api/reports/committee", MOD).status == 403, "Moderator lacks the reports permission");
    CHECK(call("GET", "/api/reports/committee?committeeId=COM002", SCRIBE).status == 403, "EB can't report on another committee");
    CHECK(call("GET", "/api/reports/committee", A).code() == "VALIDATION", "committee report needs a committee");
    Result cr = call("GET", "/api/reports/committee", SCRIBE);
    CHECK(cr.status == 200 && cr.data()["scope"].asString() == "UNGA" && cr.data()["rows"].size() == 3, "EB report defaults to own committee");
    const Json& india = cr.data()["rows"].asArray()[0];
    CHECK(india.asArray()[0].asString() == "India" && india.asArray()[3].asString() == "1" && india.asArray()[8].asString() == "1",
          "India: one speech, one ballot");
    const Json& brazil = cr.data()["rows"].asArray()[1];
    CHECK(brazil.asArray()[5].asString() == "1" && brazil.asArray()[6].asString() == "1" && brazil.asArray()[7].asString() == "1",
          "Brazil: POI raised and accepted, one motion");
    Result conference = call("GET", "/api/reports/conference", A);
    CHECK(conference.data()["rows"].asArray()[0].asArray()[3].asString() == "3", "conference report: 3 placed in UNGA");
    Result sc = call("GET", "/api/reports/scoring", A);
    double histTotal = 0;
    for (const Json& v : sc.data()["chart"]["values"].asArray()) histTotal += v.asNumber();
    std::string mean;
    for (const Json& s : sc.data()["summary"].asArray())
        if (s["label"].asString() == "Confirmed score % mean") mean = s["value"].asString();
    CHECK(histTotal == 2 && mean == "75.0", "scoring statistics (C): mean of 78% and 72% is 75.0; histogram counts both");
    Result csv = call("GET", "/api/reports/delegates?format=csv", A);
    CHECK(csv.type.rfind("text/csv", 0) == 0 && csv.disposition.find(".csv") != std::string::npos, "CSV download headers");
    CHECK(csv.raw.rfind("\xEF\xBB\xBFID,Name,", 0) == 0, "CSV starts with a BOM and the header row");
    CHECK(csv.raw.find("\"Sharma, \"\"Rahul\"\"\"") != std::string::npos, "comma and quotes escaped per RFC 4180");
    Result txt = call("GET", "/api/reports/voting?format=txt", A);
    std::istringstream lines(txt.raw);
    std::string l, header, rule;
    while (std::getline(lines, l)) { if (l.rfind("ID", 0) == 0) { header = l; std::getline(lines, rule); break; } }
    CHECK(txt.raw.rfind("MUNSphere: Voting", 0) == 0 && !header.empty() && rule.find_first_not_of("- ") == std::string::npos &&
              rule.size() == header.size(),
          "plain-text export with aligned columns");
    CHECK(call("GET", "/api/reports/nonsense", A).status == 404, "unknown report");
    CHECK(call("GET", "/api/reports/voting?format=pdf", A).status == 400, "unknown format");
    Result an = call("GET", "/api/analytics/dsa", A);
    const Json::Array& s4 = an.data()["searches"].asArray();
    CHECK(s4.size() == 4 && s4[0]["searches"].asNumber() == 3 && s4[0]["averageComparisons"].asNumber() <= s4[1]["averageComparisons"].asNumber(),
          "analytics: every delegate searched by all four methods");
    CHECK(an.data()["sorting"]["identicalRankings"].asBool(), "analytics: five sorts agree on real data");
    CHECK(call("GET", "/api/analytics/dsa", CHAIR).status == 403, "analytics is Admin only");

    // ---------------------------------------------------- announcements
    std::printf("Announcements\n");
    call("POST", "/api/announcements", A, R"({"title":"Welcome","body":"Opening at 10.","audience":"all"})");
    call("POST", "/api/announcements", A, R"({"title":"Delegates only","body":"Dress code.","audience":"delegates"})");
    call("POST", "/api/announcements", A, R"({"title":"EB briefing","body":"At 9.","audience":"eb","pinned":true})");
    call("POST", "/api/announcements", A, R"({"title":"AIPPM room change","body":"Room 4.","audience":"committee","committeeId":"COM002"})");
    CHECK(call("POST", "/api/announcements", A, R"({"title":"x","body":"y","audience":"committee","committeeId":"COM999"})").status == 404,
          "unknown committee refused");
    CHECK(call("POST", "/api/announcements", CHAIR, R"({"title":"x","body":"y"})").status == 403, "only Admin posts");
    CHECK(call("GET", "/api/announcements", D[1]).data().size() == 2, "delegate: all + delegates");
    Result ebAnn = call("GET", "/api/announcements", SCRIBE);
    CHECK(ebAnn.data().size() == 2 && ebAnn.data().asArray()[0]["title"].asString() == "EB briefing", "EB: pinned first");
    CHECK(call("GET", "/api/announcements", A).data().size() == 4, "Admin sees all");
    CHECK(call("DELETE", "/api/announcements/AN001", A).status == 200 && call("GET", "/api/announcements", D[1]).data().size() == 1, "delete");

    // ------------------------------------------------- backup and restore
    std::printf("Backup, restore and rollback\n");
    Result st = call("GET", "/api/persistence/status", A);
    bool accountsOk = false;
    for (const Json& f : st.data()["files"].asArray())
        if (f["file"].asString() == "accounts.txt" && f["exists"].asBool() && f["valid"].asBool() && f["records"].asNumber() == 7) accountsOk = true;
    CHECK(st.data()["files"].size() == 22 && accountsOk, "status lists every data file with record counts");
    Result bk = call("POST", "/api/backups", A, R"({"label":"Before day 2"})");
    std::string b1 = bk.data()["id"].asString();
    CHECK(bk.status == 201 && bk.data()["files"].asNumber() >= 20, "backup created");
    call("POST", "/api/conferences", A, R"({"name":"Added after backup","venue":"V","startDate":"2026-12-05","endDate":"2026-12-06"})");
    CHECK(conference_count() == 2, "a change after the backup");
    Result rs = call("POST", "/api/backups/" + b1 + "/restore", A);
    CHECK(rs.status == 200 && conference_count() == 1, "restore brings back the earlier state");
    CHECK(call("GET", "/api/backups", A).data().size() == 2, "a safety backup was kept automatically");
    CHECK(call("GET", "/api/audit?q=restored%20backup", A).data()["entries"].size() == 1, "the restore itself is in the audit log");
    bool kept = false;
    Result hist = call("GET", "/api/audit?q=new%3DCONF002", A);   // keep the result alive while iterating
    for (const Json& e : hist.data()["entries"].asArray())
        if (e["action"].asString() == "Created conference" && e["status"].asNumber() == 201) kept = true;
    CHECK(kept, "the audit log keeps its full history: the change made after the backup is still recorded");
    CHECK(call("GET", "/api/audit/verify", A).data()["intact"].asBool(), "chain still intact after restore");
    CHECK(!login("D001", "Delegate123").empty(), "accounts work after restore");
    { std::ofstream tamper(dir + "/backups/" + b1 + "/conferences.txt", std::ios::app); tamper << "CONF777|Injected|X|2026-12-01|2026-12-02|\n"; }
    Result bad = call("POST", "/api/backups/" + b1 + "/restore", A);
    CHECK(bad.code() == "BACKUP_INVALID" && conference_count() == 1, "altered backup refused; nothing changed");
    CHECK(call("POST", "/api/backups/..%2F..%2Fetc/restore", A).status == 400, "path tricks refused");
    CHECK(call("POST", "/api/backups", CHAIR, "{}").status == 403, "backups are Admin only");

    std::printf("Round trip and integrity\n");
    Result vf = call("POST", "/api/persistence/verify", A);
    bool allMatch = true;
    for (const Json& f : vf.data()["files"].asArray()) {
        if (!f["match"].asBool()) std::printf("  round trip differs: %s\n", f["file"].asString().c_str());
        allMatch = allMatch && f["match"].asBool();
    }
    CHECK(vf.data()["roundTripOk"].asBool() && allMatch, "save -> load -> save gives the same files");
    CHECK(vf.data()["integrity"]["errors"].asNumber() == 0, "no integrity errors");
    CHECK(delegate_count() == 3 && sheet_count() == 2 && audit_count() > 50, "memory intact after the check");

    std::printf("Tamper detection and orphans\n");
    std::string path = dir + "/audit_log.txt", content = readFile(path);
    std::size_t at = content.find("Created conference");
    content.replace(at, 18, "Deleted conference");
    { std::ofstream out(path, std::ios::binary | std::ios::trunc); out << content; }
    audit_load(path.c_str(), nullptr);
    Result tv = call("GET", "/api/audit/verify", A);
    CHECK(!tv.data()["intact"].asBool() && tv.data()["brokenAtSeq"].asNumber() >= 1, "an edited past entry is detected");
    conference_delete("CONF001");  // bypass every rule, as a hand edit would
    Result ig = call("POST", "/api/persistence/verify", A);
    CHECK(ig.data()["integrity"]["errors"].asNumber() == 2, "orphaned committees reported");

    resetAll();
    fs::remove_all(dir);
    std::printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
