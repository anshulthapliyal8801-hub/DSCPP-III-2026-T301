// MUNSphere — C++ tests for the shared layer and Module 1.
// Requests go through the real Router and real ConferenceManager, with the
// real C store and real files, but no network. Run from the project root.
#include <cstdio>
#include <filesystem>
#include <string>

#include "API.h"
#include "Auth.h"
#include "Conference.h"
#include "Database.h"
#include "dsa_api.h"

using namespace mun;
namespace fs = std::filesystem;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (cond) { g_pass++; }                                            \
        else { g_fail++; std::printf("  FAIL line %d: %s\n", __LINE__, msg); } \
    } while (0)

static std::string g_token;  // admin bearer token for every call

struct Result {
    int status;
    Json body;
};

static Result call(Router& r, const std::string& method, const std::string& path,
                   const std::string& body = "") {
    HttpRequest req;
    req.method = method;
    std::size_t q = path.find('?');
    req.path = path.substr(0, q);
    if (q != std::string::npos) req.query = parseQueryString(path.substr(q + 1));
    req.body = body;
    if (!g_token.empty()) req.headers["authorization"] = "Bearer " + g_token;
    HttpResponse res = r.dispatch(req);
    return {res.status, Json::parse(res.body)};
}

static std::string errCode(const Result& r) { return r.body["error"]["code"].asString(); }

static void testJson() {
    std::printf("JSON parser and serializer\n");
    Json j = Json::parse(R"({"a":1,"b":[true,false,null],"c":"x\"y\\z\n","d":-2.5e2})");
    CHECK(j["a"].asNumber() == 1, "integer");
    CHECK(j["b"].size() == 3 && j["b"].asArray()[0].asBool(), "array of literals");
    CHECK(j["c"].asString() == "x\"y\\z\n", "string escapes");
    CHECK(j["d"].asNumber() == -250, "exponent");
    CHECK(Json::parse(j.dump()).dump() == j.dump(), "dump/parse round-trip");
    CHECK(Json::parse(R"("\u00e9")").asString() == "\xC3\xA9", "\\u escape to UTF-8");
    CHECK(Json::parse(R"("\ud83c\udf0d")").asString() == "\xF0\x9F\x8C\x8D", "surrogate pair");
    CHECK(j["missing"].isNull(), "missing key is null");
    Json n = Json::object();
    n.set("i", 42).set("f", 0.5);
    CHECK(n.dump() == R"({"i":42,"f":0.5})", "number formatting keeps insertion order");

    const char* bad[] = {"", "{", "[1,]", R"({"a" 1})", "tru", R"("abc)", "01", "1.",
                         R"("\x")", R"("\ud800")", "[1] 2", "1e99999"};
    int rejected = 0;
    for (const char* b : bad) {
        try { Json::parse(b); } catch (const JsonError&) { rejected++; }
    }
    CHECK(rejected == 12, "all malformed inputs rejected with JsonError");

    std::string deep(100, '[');
    deep += std::string(100, ']');
    bool depthRejected = false;
    try { Json::parse(deep); } catch (const JsonError&) { depthRejected = true; }
    CHECK(depthRejected, "nesting deeper than 64 rejected");
}

static void testValidation() {
    std::printf("Validation helpers\n");
    CHECK(validate::isDate("2028-02-29"), "leap day valid");
    CHECK(!validate::isDate("2026-02-29"), "non-leap Feb 29 invalid");
    CHECK(!validate::isDate("2026-13-01") && !validate::isDate("2026-1-01"), "bad month/format");
    CHECK(validate::isTime("00:00") && validate::isTime("23:59"), "time bounds");
    CHECK(!validate::isTime("24:00") && !validate::isTime("9:00"), "bad times");
    CHECK(validate::isId("CONF001") && !validate::isId("conf001") && !validate::isId("A"),
          "ID rules");
    CHECK(validate::trim("  a b  ") == "a b", "trim");
}

static void testRouter(Router& r) {
    std::printf("Router\n");
    CHECK(call(r, "GET", "/api/nowhere").status == 404, "unknown path is 404");
    CHECK(call(r, "PATCH", "/api/conferences").status == 405, "wrong method is 405");
    Result bad = call(r, "POST", "/api/conferences", "{not json");
    CHECK(bad.status == 400 && errCode(bad) == "BAD_JSON", "malformed body is 400 BAD_JSON");
    Result arr = call(r, "POST", "/api/conferences", "[1,2]");
    CHECK(arr.status == 400, "array body rejected");
    Result empty = call(r, "POST", "/api/conferences", "");
    CHECK(empty.status == 400, "empty body rejected");
}

static void testConferences(Router& r) {
    std::printf("Conferences\n");
    Result c = call(r, "POST", "/api/conferences",
                    R"({"name":"MUNSphere 2026","venue":"Graphic Era","startDate":"2026-12-01","endDate":"2026-12-03"})");
    CHECK(c.status == 201, "create returns 201");
    CHECK(c.body["data"]["id"].asString() == "CONF001", "auto ID CONF001");

    Result dup = call(r, "POST", "/api/conferences",
                      R"({"id":"CONF001","name":"X","venue":"Y","startDate":"2026-12-01","endDate":"2026-12-01"})");
    CHECK(dup.status == 409 && errCode(dup) == "DUPLICATE_ID", "duplicate ID is 409");

    Result badId = call(r, "POST", "/api/conferences",
                        R"({"id":"bad id","name":"X","venue":"Y","startDate":"2026-12-01","endDate":"2026-12-01"})");
    CHECK(badId.status == 400, "malformed custom ID rejected");

    Result order = call(r, "POST", "/api/conferences",
                        R"({"name":"X","venue":"Y","startDate":"2026-12-05","endDate":"2026-12-01"})");
    CHECK(order.status == 400, "end before start rejected");

    Result missing = call(r, "POST", "/api/conferences", R"({"name":"X"})");
    CHECK(missing.status == 400 && errCode(missing) == "VALIDATION", "missing field rejected");

    Result longName = call(r, "POST", "/api/conferences",
                           "{\"name\":\"" + std::string(101, 'a') +
                               "\",\"venue\":\"Y\",\"startDate\":\"2026-12-01\",\"endDate\":\"2026-12-01\"}");
    CHECK(longName.status == 400, "over-length name rejected, not truncated");

    Result upd = call(r, "PUT", "/api/conferences/CONF001", R"({"venue":"Main Auditorium"})");
    CHECK(upd.status == 200 && upd.body["data"]["venue"].asString() == "Main Auditorium" &&
              upd.body["data"]["name"].asString() == "MUNSphere 2026",
          "partial update keeps other fields");
    CHECK(call(r, "PUT", "/api/conferences/CONF001", R"({"id":"CONF009"})").status == 400,
          "ID change rejected");
    CHECK(call(r, "GET", "/api/conferences/NOPE").status == 404, "missing conference is 404");
}

static void testCommittees(Router& r, ConferenceManager& m) {
    std::printf("Committees\n");
    Result c = call(r, "POST", "/api/committees",
                    R"({"conferenceId":"CONF001","name":"General Assembly","abbreviation":"UNGA","capacity":60})");
    CHECK(c.status == 201 && c.body["data"]["id"].asString() == "COM001", "create COM001");
    call(r, "POST", "/api/committees",
         R"({"conferenceId":"CONF001","name":"All India Political Parties Meet","abbreviation":"AIPPM","capacity":40})");

    Result noConf = call(r, "POST", "/api/committees",
                         R"({"conferenceId":"CONF404","name":"X","abbreviation":"X","capacity":10})");
    CHECK(noConf.status == 400 && errCode(noConf) == "INVALID_REFERENCE", "unknown conference rejected");

    Result dupAbbr = call(r, "POST", "/api/committees",
                          R"({"conferenceId":"CONF001","name":"Other","abbreviation":"unga","capacity":10})");
    CHECK(dupAbbr.status == 409, "duplicate abbreviation (any case) rejected");

    CHECK(call(r, "POST", "/api/committees",
               R"({"conferenceId":"CONF001","name":"X","abbreviation":"X","capacity":0})").status == 400,
          "capacity 0 rejected");
    CHECK(call(r, "POST", "/api/committees",
               R"({"conferenceId":"CONF001","name":"X","abbreviation":"X","capacity":2.5})").status == 400,
          "fractional capacity rejected");

    Result search = call(r, "GET", "/api/committees?q=parties");
    CHECK(search.status == 200 && search.body["data"].size() == 1 &&
              search.body["data"].asArray()[0]["id"].asString() == "COM002",
          "search by name");

    CHECK(call(r, "PUT", "/api/committees/COM001", R"({"conferenceId":"CONF999"})").status == 400,
          "moving committee to another conference rejected");

    // Conference with committees can't be deleted
    Result delConf = call(r, "DELETE", "/api/conferences/CONF001");
    CHECK(delConf.status == 409 && errCode(delConf) == "HAS_DEPENDENTS",
          "conference with committees is protected");

    // Registered dependency checks are honoured (used by later modules)
    m.addCommitteeDependencyCheck([](const std::string& id) {
        return id == "COM002" ? std::string("COM002 has delegates assigned.") : std::string();
    });
    Result delCom = call(r, "DELETE", "/api/committees/COM002");
    CHECK(delCom.status == 409 && delCom.body["error"]["message"].asString() == "COM002 has delegates assigned.",
          "external dependency check blocks deletion");
}

static void testSessions(Router& r) {
    std::printf("Sessions\n");
    Result s1 = call(r, "POST", "/api/sessions",
                     R"({"committeeId":"COM001","title":"Opening","date":"2026-12-01","startTime":"10:00","endTime":"12:00"})");
    CHECK(s1.status == 201 && s1.body["data"]["status"].asString() == "scheduled", "create scheduled");
    call(r, "POST", "/api/sessions",
         R"({"committeeId":"COM001","title":"Morning","date":"2026-12-01","startTime":"08:00","endTime":"09:30"})");

    Result list = call(r, "GET", "/api/sessions?committeeId=COM001");
    CHECK(list.body["data"].size() == 2 &&
              list.body["data"].asArray()[0]["id"].asString() == "SES002",
          "sessions listed in time order (sorted linked list)");

    CHECK(call(r, "POST", "/api/sessions",
               R"({"committeeId":"COM001","title":"X","date":"2026-12-09","startTime":"10:00","endTime":"11:00"})").status == 400,
          "date outside conference rejected");
    Result overlap = call(r, "POST", "/api/sessions",
                          R"({"committeeId":"COM001","title":"X","date":"2026-12-01","startTime":"11:30","endTime":"13:00"})");
    CHECK(overlap.status == 409 && errCode(overlap) == "TIME_CONFLICT", "overlap rejected");
    CHECK(call(r, "POST", "/api/sessions",
               R"({"committeeId":"COM001","title":"X","date":"2026-12-01","startTime":"12:00","endTime":"13:00"})").status == 201,
          "back-to-back session allowed");
    CHECK(call(r, "POST", "/api/sessions",
               R"({"committeeId":"COM001","title":"X","date":"2026-12-02","startTime":"11:00","endTime":"10:00"})").status == 400,
          "end before start rejected");
    CHECK(call(r, "POST", "/api/sessions",
               R"({"committeeId":"COM404","title":"X","date":"2026-12-02","startTime":"10:00","endTime":"11:00"})").status == 400,
          "unknown committee rejected");

    // State machine
    CHECK(call(r, "POST", "/api/sessions/SES001/activate").status == 200, "activate");
    Result second = call(r, "POST", "/api/sessions/SES002/activate");
    CHECK(second.status == 409 && errCode(second) == "ALREADY_ACTIVE", "one active session per committee");
    CHECK(call(r, "PUT", "/api/sessions/SES001", R"({"title":"Renamed"})").status == 409,
          "active session can't be edited");
    CHECK(call(r, "DELETE", "/api/sessions/SES001").status == 409, "active session can't be deleted");
    CHECK(call(r, "POST", "/api/sessions/SES001/deactivate").status == 200, "deactivate");
    CHECK(call(r, "POST", "/api/sessions/SES001/complete").status == 409,
          "can't complete a session that isn't active");
    call(r, "POST", "/api/sessions/SES001/activate");
    Result done = call(r, "POST", "/api/sessions/SES001/complete");
    CHECK(done.status == 200 && done.body["data"]["status"].asString() == "completed", "complete");
    CHECK(call(r, "POST", "/api/sessions/SES001/activate").status == 409, "completed can't reactivate");

    Result moved = call(r, "PUT", "/api/sessions/SES002", R"({"date":"2026-12-02"})");
    CHECK(moved.status == 200, "reschedule a scheduled session");

    Result shrink = call(r, "PUT", "/api/conferences/CONF001", R"({"endDate":"2026-12-01"})");
    CHECK(shrink.status == 409 && errCode(shrink) == "SESSION_OUTSIDE_DATES",
          "conference dates can't exclude an existing session");

    Result delCom = call(r, "DELETE", "/api/committees/COM001");
    CHECK(delCom.status == 409, "committee with sessions is protected");

    Result filtered = call(r, "GET", "/api/sessions?status=completed");
    CHECK(filtered.body["data"].size() == 1, "status filter");
    CHECK(call(r, "GET", "/api/sessions?status=paused").status == 400, "bad status filter rejected");

    Result ov = call(r, "GET", "/api/overview");
    CHECK(ov.status == 200 && ov.body["data"]["counts"]["sessions"].asNumber() == 3, "overview counts");
}

static void testDsaTrace(Router& r) {
    std::printf("DSA trace endpoints\n");
    Result t = call(r, "GET", "/api/dsa/module1/search?type=committee&id=COM002");
    CHECK(t.status == 200 && t.body["data"]["found"].asBool(), "trace finds COM002");
    CHECK(t.body["data"]["comparisons"].asNumber() == 2, "two comparisons for 2nd node");
    CHECK(t.body["data"]["path"].size() == 2 &&
              t.body["data"]["path"].asArray()[1].asString() == "COM002",
          "path ends at target");
    Result miss = call(r, "GET", "/api/dsa/module1/search?type=committee&id=COM999");
    CHECK(!miss.body["data"]["found"].asBool() && miss.body["data"]["comparisons"].asNumber() == 2,
          "unsuccessful search examines every node");
    CHECK(call(r, "GET", "/api/dsa/module1/search?type=planet&id=X").status == 400, "bad type rejected");
}

static void testPersistence(const std::string& dir) {
    std::printf("Persistence across restart\n");
    int before = session_count();
    m1_store_reset();
    CHECK(session_count() == 0, "store cleared (simulated restart)");
    Database::instance().open(dir);
    CHECK(conference_count() == 1 && committee_count() == 2 && session_count() == before,
          "everything reloaded from disk");
    SessionRec s;
    CHECK(session_find("SES001", &s, nullptr) == DSA_OK && s.status == SESSION_COMPLETED,
          "session status survived restart");
}

int main() {
    std::printf("=== Module 1 C++ / API tests ===\n");
    const std::string dir = "tests/tmp_data";
    fs::remove_all(dir);
    m1_store_reset();
    Database::instance().open(dir);

    account_store_reset();
    Auth::Config cfg;
    cfg.iterations = 1000;  // fast for tests; production uses 60,000
    Auth auth(cfg);
    auth.createAccount(ROLE_ADMIN, "Test Admin", "Admin12345", false);
    g_token = auth.login("ADM001", "Admin12345")["token"].asString();

    ConferenceManager m;
    Router r;
    registerModule1Routes(r, m, auth);

    testJson();
    testValidation();
    testRouter(r);
    testConferences(r);
    testCommittees(r, m);
    testSessions(r);
    testDsaTrace(r);
    testPersistence(dir);

    m1_store_reset();
    account_store_reset();
    fs::remove_all(dir);
    std::printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
