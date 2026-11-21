// MUNSphere — C++ tests for Phase 2 (authentication, roles) and Phase 3
// (delegates, committee assignment), through the real routes and stores.
// Includes every authorization rule listed in section 12 of the spec.
#include <cstdio>
#include <filesystem>
#include <string>

#include "API.h"
#include "Auth.h"
#include "CommitteeManager.h"
#include "Conference.h"
#include "Database.h"
#include "DelegateManager.h"
#include "EBManager.h"
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

static Result call(const std::string& method, const std::string& path, const std::string& token = "",
                   const std::string& body = "") {
    HttpRequest req;
    req.method = method;
    std::size_t q = path.find('?');
    req.path = path.substr(0, q);
    if (q != std::string::npos) req.query = parseQueryString(path.substr(q + 1));
    req.body = body;
    if (!token.empty()) req.headers["authorization"] = "Bearer " + token;
    HttpResponse res = g_router->dispatch(req);
    return {res.status, Json::parse(res.body)};
}

static std::string loginToken(const std::string& id, const std::string& pw) {
    Result r = call("POST", "/api/auth/login", "", R"({"id":")" + id + R"(","password":")" + pw + R"("})");
    return r.status == 200 ? r.data()["token"].asString() : "";
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== Phase 2 and 3 tests: auth, roles, delegates, EB ===\n");
    const std::string dir = "tests/tmp_auth";
    fs::remove_all(dir);
    m1_store_reset(); account_store_reset(); eb_store_reset(); delegate_store_reset();
    Database::instance().open(dir);

    Auth::Config cfg;
    cfg.iterations = 1000;
    cfg.maxFailures = 3;
    Auth auth(cfg);
    ConferenceManager conf;
    EBManager eb(auth, conf);
    DelegateManager dm(auth, conf);
    CommitteeManager cm(dm, conf);
    Router router;
    registerAuthRoutes(router, auth);
    registerModule1Routes(router, conf, auth);
    registerDelegateRoutes(router, dm, auth);
    registerCommitteeRoutes(router, cm, auth);
    registerEBRoutes(router, eb, auth);
    g_router = &router;

    // ------------------------------------------------------------------
    std::printf("Unauthenticated access\n");
    CHECK(call("GET", "/api/conferences").status == 401, "reading needs login");
    CHECK(call("POST", "/api/conferences", "", "{}").code() == "AUTH_REQUIRED", "writing needs login");
    CHECK(call("GET", "/api/delegates", "forged-token").status == 401, "unknown token rejected");
    CHECK(call("GET", "/api/public/live").status == 200, "public live endpoint open");
    Result rr1 = call("POST", "/api/auth/request-reset", "", R"({"id":"D999"})");
    CHECK(rr1.status == 200, "reset request for unknown ID gives the same answer");

    std::printf("Admin login and lockout\n");
    auth.createAccount(ROLE_ADMIN, "Asha Admin", "Admin12345", false);
    CHECK(call("POST", "/api/auth/login", "", R"({"id":"ADM001","password":"wrong"})").status == 401, "wrong password");
    std::string admin = loginToken("adm001", "Admin12345");
    CHECK(!admin.empty(), "login (ID is case-insensitive)");
    CHECK(call("GET", "/api/auth/me", admin).data()["role"].asString() == "admin", "me reports admin");
    for (int i = 0; i < 3; ++i) call("POST", "/api/auth/login", "", R"({"id":"D777","password":"x"})");
    CHECK(call("POST", "/api/auth/login", "", R"({"id":"D777","password":"x"})").status == 429, "locked after 3 failures");
    {
        AccountRec a;
        account_find("ADM001", &a, nullptr);
        CHECK(std::string(a.passHash).rfind("pbkdf2-sha256$1000$", 0) == 0, "password stored as PBKDF2 hash");
        CHECK(std::string(a.passHash).find("Admin12345") == std::string::npos, "plain password never stored");
    }

    // Conference data
    call("POST", "/api/conferences", admin, R"({"name":"MUNSphere 2026","venue":"Graphic Era","startDate":"2026-12-01","endDate":"2026-12-03"})");
    call("POST", "/api/conferences", admin, R"({"name":"Other MUN","venue":"Elsewhere","startDate":"2026-12-10","endDate":"2026-12-11"})");
    call("POST", "/api/committees", admin, R"({"conferenceId":"CONF001","name":"General Assembly","abbreviation":"UNGA","capacity":2})");
    call("POST", "/api/committees", admin, R"({"conferenceId":"CONF001","name":"All India Political Parties Meet","abbreviation":"AIPPM","capacity":40})");
    call("POST", "/api/committees", admin, R"({"conferenceId":"CONF002","name":"Security Council","abbreviation":"UNSC","capacity":15})");

    // ------------------------------------------------------------------
    std::printf("Delegate registration\n");
    CHECK(call("POST", "/api/auth/register", "", R"({"name":"Rahul Sharma","email":"rahul@x.in","password":"short"})").code() == "WEAK_PASSWORD",
          "weak password rejected");
    CHECK(call("POST", "/api/auth/register", "", R"({"name":"Rahul Sharma","email":"not-an-email","password":"Delegate123"})").status == 400,
          "bad email rejected");
    Result reg = call("POST", "/api/auth/register", "", R"({"name":"Rahul Sharma","email":"Rahul@X.in","institution":"GEU","password":"Delegate123"})");
    CHECK(reg.status == 201 && reg.data()["user"]["id"].asString() == "D001", "registered as D001 and signed in");
    std::string d1 = reg.data()["token"].asString();
    CHECK(call("POST", "/api/auth/register", "", R"({"name":"Copy","email":"rahul@x.in","password":"Delegate123"})").code() == "DUPLICATE_EMAIL",
          "duplicate email (any case) rejected");
    std::string d2 = call("POST", "/api/auth/register", "", R"({"name":"Meera Iyer","email":"meera@x.in","password":"Delegate123"})").data()["token"].asString();
    std::string d3 = call("POST", "/api/auth/register", "", R"({"name":"Arjun Rao","email":"arjun@x.in","password":"Delegate123"})").data()["token"].asString();

    std::printf("Delegates can't reach Admin or EB functions\n");
    CHECK(call("GET", "/api/delegates", d1).status == 403, "no delegate list");
    CHECK(call("POST", "/api/conferences", d1, "{}").status == 403, "no conference creation");
    CHECK(call("GET", "/api/overview", d1).status == 403, "no admin overview");
    CHECK(call("GET", "/api/sessions", d1).status == 403, "no raw session list");
    CHECK(call("POST", "/api/eb", d1, "{}").status == 403, "can't create EB accounts");
    CHECK(call("POST", "/api/delegates/D001/approve", d1).status == 403, "can't approve own registration");
    CHECK(call("POST", "/api/delegates/D001/assign", d1, R"({"committeeId":"COM001","portfolio":"India"})").status == 403,
          "can't change own assignment");
    CHECK(call("GET", "/api/committees/COM001/roster", d1).status == 403, "can't see rosters");
    CHECK(call("GET", "/api/admin/accounts", d1).status == 403, "can't list accounts");
    CHECK(call("GET", "/api/conferences", d1).status == 200, "can view available conferences");

    std::printf("Conference registration workflow\n");
    CHECK(call("POST", "/api/delegate/me/registration", d1, R"({"conferenceId":"CONF001","preferences":["COM003"]})").status == 400,
          "preference from another conference rejected");
    CHECK(call("POST", "/api/delegate/me/registration", d1, R"({"conferenceId":"CONF001","preferences":["COM001","COM001"]})").status == 400,
          "duplicate preference rejected");
    Result ap = call("POST", "/api/delegate/me/registration", d1, R"({"conferenceId":"CONF001","preferences":["COM001","COM002"]})");
    CHECK(ap.status == 200 && ap.data()["status"].asString() == "pending", "applied: pending");
    call("POST", "/api/delegate/me/registration", d2, R"({"conferenceId":"CONF001","preferences":["COM001"]})");
    call("POST", "/api/delegate/me/registration", d3, R"({"conferenceId":"CONF001","preferences":["COM001"]})");
    CHECK(call("POST", "/api/delegates/D002/assign", admin, R"({"committeeId":"COM001","portfolio":"Brazil"})").code() == "INVALID_STATE",
          "can't assign before approval");
    CHECK(call("POST", "/api/delegates/D001/approve", admin).data()["status"].asString() == "approved", "approve");
    CHECK(call("POST", "/api/delegates/D002/reject", admin, "{}").status == 400, "rejection needs a reason");
    call("POST", "/api/delegates/D002/reject", admin, R"({"reason":"Incomplete institution details"})");
    Result me2 = call("GET", "/api/delegate/me", d2);
    CHECK(me2.data()["status"].asString() == "rejected" && me2.data()["note"].asString() == "Incomplete institution details",
          "delegate sees rejection reason");
    call("POST", "/api/delegate/me/registration", d2, R"({"conferenceId":"CONF001","preferences":["COM001"]})");
    CHECK(call("GET", "/api/delegate/me", d2).data()["status"].asString() == "pending", "re-apply after rejection");
    CHECK(call("DELETE", "/api/delegate/me/registration", d1).status == 409, "approved registration can't be withdrawn");
    call("POST", "/api/delegates/D002/approve", admin);
    call("POST", "/api/delegates/D003/approve", admin);

    std::printf("Committee assignment: portfolio, capacity, conference\n");
    CHECK(call("POST", "/api/delegates/D001/assign", admin, R"({"committeeId":"COM001","portfolio":"India"})").status == 200, "assign D001");
    CHECK(call("POST", "/api/delegates/D002/assign", admin, R"({"committeeId":"COM001","portfolio":"india"})").code() == "PORTFOLIO_TAKEN",
          "same portfolio twice in a committee rejected");
    CHECK(call("POST", "/api/delegates/D002/assign", admin, R"({"committeeId":"COM001","portfolio":"Brazil"})").status == 200, "assign D002");
    CHECK(call("POST", "/api/delegates/D003/assign", admin, R"({"committeeId":"COM001","portfolio":"Chile"})").code() == "COMMITTEE_FULL",
          "capacity enforced");
    CHECK(call("POST", "/api/delegates/D003/assign", admin, R"({"committeeId":"COM003","portfolio":"Chile"})").code() == "WRONG_CONFERENCE",
          "committee must be in the delegate's conference");
    CHECK(call("POST", "/api/delegates/D003/assign", admin, R"({"committeeId":"COM002","portfolio":"BJP"})").status == 200, "assign D003");
    CHECK(call("PUT", "/api/committees/COM001", admin, R"({"capacity":1})").code() == "CAPACITY", "capacity can't drop below assigned");
    CHECK(call("DELETE", "/api/committees/COM001", admin).code() == "HAS_DEPENDENTS", "committee with delegates protected");
    Result roster = call("GET", "/api/committees/COM001/roster?sort=portfolio", admin);
    CHECK(roster.data()["delegates"].size() == 2 &&
              roster.data()["delegates"].asArray()[0]["portfolio"].asString() == "Brazil",
          "roster sorted by portfolio (C insertion sort)");
    CHECK(call("GET", "/api/delegates?status=assigned", admin).data().size() == 3, "filter assigned");
    Result alpha = call("GET", "/api/delegates", admin);
    CHECK(alpha.data().asArray()[0]["name"].asString() == "Arjun Rao" &&
              alpha.data().asArray()[2]["name"].asString() == "Rahul Sharma",
          "list is alphabetical (BST in-order)");
    CHECK(call("GET", "/api/delegates?q=meera", admin).data().size() == 1, "text search");
    CHECK(call("DELETE", "/api/delegates/D001", admin).code() == "HAS_DEPENDENTS", "assigned delegate can't be deleted");

    // ------------------------------------------------------------------
    std::printf("Executive Board accounts\n");
    Result e1 = call("POST", "/api/eb", admin, R"({"name":"Priya Chair","email":"priya@x.in","ebRole":"Chairperson","committeeId":"COM001"})");
    CHECK(e1.status == 201 && e1.data()["member"]["id"].asString() == "EB001", "EB001 created");
    CHECK(e1.data()["temporaryPassword"].asString().size() == 12, "12-character temporary password");
    CHECK(e1.data()["member"]["permissions"].size() == 10, "Chairperson gets every permission (overridden default)");
    std::string t1 = e1.data()["temporaryPassword"].asString();
    Result e2 = call("POST", "/api/eb", admin, R"({"name":"Kabir Mod","email":"kabir@x.in","ebRole":"moderator","committeeId":"COM001"})");
    Result e3 = call("POST", "/api/eb", admin, R"({"name":"Sara Scribe","email":"sara@x.in","ebRole":"Scribe","committeeId":"COM002"})");
    Result e4 = call("POST", "/api/eb", admin, R"({"name":"Dev Chair","email":"dev@x.in","ebRole":"Chairperson","committeeId":"COM002"})");
    CHECK(e3.data()["member"]["permissions"].size() == 3, "Scribe defaults: view, attendance, reports");
    CHECK(call("POST", "/api/eb", admin, R"({"name":"X","email":"x@x.in","ebRole":"Secretary","committeeId":"COM001"})").status == 400,
          "unknown EB role rejected");
    CHECK(call("POST", "/api/eb", admin, R"({"name":"Xy","email":"x@x.in","ebRole":"Scribe","committeeId":"COM404"})").status == 400,
          "unknown committee rejected");

    std::string eb1 = loginToken("EB001", t1);
    CHECK(call("GET", "/api/sessions", eb1).code() == "PASSWORD_CHANGE_REQUIRED", "temporary password forces a change");
    CHECK(call("POST", "/api/auth/change-password", eb1, R"({"currentPassword":"nope","newPassword":"Chair2026x"})").code() == "WRONG_PASSWORD",
          "change needs the current password");
    CHECK(call("POST", "/api/auth/change-password", eb1, R"({"currentPassword":")" + t1 + R"(","newPassword":"Chair2026x"})").status == 200,
          "password changed");
    auto activate = [](const std::string&, const std::string& pw, const std::string& id) {
        std::string t = loginToken(id, pw);
        call("POST", "/api/auth/change-password", t, R"({"currentPassword":")" + pw + R"(","newPassword":"Board2026x"})");
        return t;
    };
    std::string eb2 = activate("", e2.data()["temporaryPassword"].asString(), "EB002");
    std::string eb3 = activate("", e3.data()["temporaryPassword"].asString(), "EB003");
    std::string eb4 = activate("", e4.data()["temporaryPassword"].asString(), "EB004");

    std::printf("EB committee scope (UNGA EB can't touch AIPPM)\n");
    Result s1 = call("POST", "/api/sessions", eb1, R"({"committeeId":"COM001","title":"Opening","date":"2026-12-01","startTime":"10:00","endTime":"12:00"})");
    CHECK(s1.status == 201, "UNGA chair creates a UNGA session");
    CHECK(call("POST", "/api/sessions", eb1, R"({"committeeId":"COM002","title":"X","date":"2026-12-01","startTime":"10:00","endTime":"12:00"})").status == 403,
          "UNGA chair can't create an AIPPM session");
    CHECK(call("GET", "/api/sessions?committeeId=COM002", eb1).status == 403, "can't list AIPPM sessions");
    CHECK(call("GET", "/api/committees/COM002/roster", eb1).status == 403, "can't see AIPPM roster");
    CHECK(call("GET", "/api/committees/COM001/roster", eb1).status == 200, "sees own roster");
    CHECK(call("GET", "/api/sessions", eb1).data().size() == 1, "session list limited to own committee");
    CHECK(call("GET", "/api/delegates", eb1).status == 403, "EB can't use Admin delegate APIs");
    CHECK(call("POST", "/api/eb", eb1, "{}").status == 403, "EB can't create EB accounts");
    CHECK(call("GET", "/api/admin/accounts", eb1).status == 403, "EB can't list accounts");

    std::printf("EB role permissions\n");
    CHECK(call("POST", "/api/sessions", eb2, R"({"committeeId":"COM001","title":"X","date":"2026-12-02","startTime":"10:00","endTime":"12:00"})").status == 403,
          "Moderator can't create sessions");
    CHECK(call("POST", "/api/sessions/SES001/activate", eb2).status == 200, "Moderator can run a session");
    call("POST", "/api/sessions", eb4, R"({"committeeId":"COM002","title":"AIPPM opening","date":"2026-12-01","startTime":"10:00","endTime":"12:00"})");
    CHECK(call("POST", "/api/sessions/SES002/activate", eb3).status == 403, "Scribe can't run sessions");
    CHECK(call("PUT", "/api/eb/EB003", admin, R"({"permissions":["run_sessions"]})").status == 200, "Admin grants a permission");
    CHECK(call("POST", "/api/sessions/SES002/activate", eb3).status == 200, "granted permission takes effect at once");
    CHECK(call("PUT", "/api/eb/EB003", admin, R"({"permissions":["teleport"]})").status == 400, "unknown permission rejected");

    std::printf("Transfer and revocation take effect immediately\n");
    CHECK(call("PUT", "/api/eb/EB002", admin, R"({"committeeId":"COM002"})").status == 200, "transfer EB002 to AIPPM");
    CHECK(call("GET", "/api/committees/COM001/roster", eb2).status == 403, "old committee closed after transfer");
    CHECK(call("GET", "/api/committees/COM002/roster", eb2).status == 200, "new committee open after transfer");
    CHECK(call("PUT", "/api/eb/EB002", admin, R"({"ebRole":"Scribe"})").data()["permissions"].size() == 3,
          "role change applies the new role's defaults");
    CHECK(call("POST", "/api/eb/EB001/revoke", admin).status == 200, "revoke EB001");
    CHECK(call("GET", "/api/committees/COM001/roster", eb1).status == 401, "revoked EB's open session stops working");
    CHECK(call("POST", "/api/auth/login", "", R"({"id":"EB001","password":"Chair2026x"})").code() == "DEACTIVATED",
          "revoked EB can't log in");
    call("POST", "/api/eb/EB001/restore", admin);
    CHECK(!loginToken("EB001", "Chair2026x").empty(), "restored EB can log in");
    CHECK(call("DELETE", "/api/committees/COM003", admin).status == 200, "committee without dependents deletes");
    CHECK(call("GET", "/api/committee-summaries", admin).data().asArray()[0]["ebCount"].asNumber() == 1,
          "summary counts EB per committee");

    std::printf("Password reset workflow\n");
    call("POST", "/api/auth/request-reset", "", R"({"id":"D003"})");
    Result accts = call("GET", "/api/admin/accounts?role=delegate", admin);
    bool flagged = false;
    for (const Json& a : accts.data().asArray())
        if (a["id"].asString() == "D003" && a["resetRequested"].asBool()) flagged = true;
    CHECK(flagged, "Admin sees the reset request");
    Result rp = call("POST", "/api/admin/accounts/D003/reset-password", admin);
    std::string temp = rp.data()["temporaryPassword"].asString();
    CHECK(rp.status == 200 && !rp.data()["account"]["resetRequested"].asBool(), "reset clears the request");
    CHECK(call("GET", "/api/delegate/me", d3).status == 401, "old session ended by reset");
    CHECK(loginToken("D003", "Delegate123").empty(), "old password no longer works");
    std::string d3b = loginToken("D003", temp);
    CHECK(call("GET", "/api/delegate/me", d3b).code() == "PASSWORD_CHANGE_REQUIRED", "temp password forces change");
    CHECK(call("POST", "/api/admin/accounts/ADM001/deactivate", admin).code() == "SELF", "Admin can't deactivate self");

    std::printf("Delegate dashboard data\n");
    Result me1 = call("GET", "/api/delegate/me", d1);
    CHECK(me1.data()["committee"]["abbreviation"].asString() == "UNGA" && me1.data()["portfolio"].asString() == "India",
          "delegate sees assigned committee and portfolio");
    CHECK(call("GET", "/api/delegate/me/sessions", d1).data().size() == 1, "delegate sees only their committee's sessions");
    CHECK(call("PUT", "/api/delegate/me", d1, R"({"name":"Rahul K Sharma"})").status == 200, "profile update");
    CHECK(call("GET", "/api/auth/history", d1).data().size() >= 1, "own login history");
    CHECK(call("GET", "/api/admin/login-history", admin).data().size() >= 5, "admin sees all login history");

    std::printf("Module 2 data-structure endpoints\n");
    Result hs = call("GET", "/api/dsa/module2/hash", admin);
    CHECK(hs.data()["size"].asNumber() == 3 && hs.data()["bucketCount"].asNumber() == 8, "hash state");
    Result bs = call("GET", "/api/dsa/module2/bst", admin);
    CHECK(bs.data()["nodeCount"].asNumber() == 3 && bs.data()["inorder"].asArray()[0].asString() == "D003",
          "BST state (Arjun first)");
    for (const char* m : {"hash", "linear", "binary"}) {
        Result t = call("GET", std::string("/api/dsa/module2/search?method=") + m + "&key=D002", admin);
        CHECK(t.data()["found"].asBool() && t.data()["comparisons"].asNumber() >= 1, "search method finds D002");
    }
    Result tb = call("GET", "/api/dsa/module2/search?method=bst&key=meera%20iyer", admin);
    CHECK(tb.data()["found"].asBool(), "BST name search");

    std::printf("Persistence across restart\n");
    m1_store_reset(); account_store_reset(); eb_store_reset(); delegate_store_reset();
    Database::instance().open(dir);
    CHECK(account_count() == 8 && eb_count() == 4 && delegate_count() == 3, "all stores reloaded");
    CHECK(!loginToken("D001", "Delegate123").empty(), "password hash survives restart");
    DelegateRec dr;
    CHECK(delegate_find("D001", &dr, nullptr) == DSA_OK && std::string(dr.name) == "Rahul K Sharma" &&
              std::string(dr.committeeId) == "COM001",
          "renamed, assigned delegate reloaded");
    EbRec er;
    CHECK(eb_find("EB002", &er, nullptr) == DSA_OK && std::string(er.committeeId) == "COM002" && er.ebRole == EB_SCRIBE,
          "EB transfer and role change reloaded");

    m1_store_reset(); account_store_reset(); eb_store_reset(); delegate_store_reset();
    fs::remove_all(dir);
    std::printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
