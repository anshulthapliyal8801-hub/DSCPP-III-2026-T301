// MUNSphere — Module12_Persistence/AuditManager.cpp
#include "AuditManager.h"

#include <cctype>
#include <map>

#include "Database.h"
#include "RoleManager.h"

namespace mun {

namespace {
const std::string kGenesis(64, '0');

// Readable names for actions; anything unlisted falls back to "METHOD /route".
const std::map<std::string, std::string>& labels() {
    static const std::map<std::string, std::string> m = {
        {"POST /api/conferences", "Created conference"}, {"PUT /api/conferences/:id", "Updated conference"},
        {"DELETE /api/conferences/:id", "Deleted conference"}, {"POST /api/committees", "Created committee"},
        {"PUT /api/committees/:id", "Updated committee"}, {"DELETE /api/committees/:id", "Deleted committee"},
        {"POST /api/sessions", "Scheduled session"}, {"PUT /api/sessions/:id", "Updated session"},
        {"DELETE /api/sessions/:id", "Deleted session"}, {"POST /api/sessions/:id/activate", "Activated session"},
        {"POST /api/sessions/:id/deactivate", "Paused session"}, {"POST /api/sessions/:id/complete", "Completed session"},
        {"POST /api/auth/register", "Delegate registered"}, {"POST /api/auth/change-password", "Changed own password"},
        {"POST /api/auth/request-reset", "Requested password reset"},
        {"POST /api/delegates/:id/approve", "Approved registration"}, {"POST /api/delegates/:id/reject", "Rejected registration"},
        {"POST /api/delegates/:id/assign", "Placed delegate"}, {"POST /api/delegates/:id/unassign", "Removed delegate from committee"},
        {"PUT /api/delegates/:id", "Edited delegate profile"}, {"DELETE /api/delegates/:id", "Deleted delegate"},
        {"PUT /api/delegate/me", "Edited own profile"}, {"POST /api/delegate/me/registration", "Registered for conference"},
        {"DELETE /api/delegate/me/registration", "Withdrew registration"},
        {"POST /api/eb", "Created EB account"}, {"PUT /api/eb/:id", "Changed EB role, committee or permissions"},
        {"POST /api/eb/:id/revoke", "Revoked EB access"}, {"POST /api/eb/:id/restore", "Restored EB access"},
        {"POST /api/admin/accounts/:id/activate", "Activated account"}, {"POST /api/admin/accounts/:id/deactivate", "Deactivated account"},
        {"POST /api/admin/accounts/:id/reset-password", "Reset password"},
        {"POST /api/sessions/:id/attendance", "Marked attendance"}, {"POST /api/sessions/:id/attendance/all", "Marked everyone else"},
        {"POST /api/sessions/:id/check-in", "Checked in"},
        {"POST /api/sessions/:id/speakers", "Added speaker"}, {"DELETE /api/sessions/:id/speakers/:delegateId", "Removed speaker"},
        {"POST /api/sessions/:id/speakers/next", "Called next speaker"}, {"POST /api/sessions/:id/speakers/finish", "Ended speech"},
        {"PUT /api/sessions/:id/speakers/time", "Set speaking time"},
        {"POST /api/sessions/:id/speak-requests", "Requested to speak"}, {"DELETE /api/sessions/:id/speak-requests", "Withdrew request"},
        {"POST /api/sessions/:id/speak-requests/approve-next", "Approved speaking request"},
        {"POST /api/sessions/:id/speak-requests/decline-next", "Declined speaking request"},
        {"POST /api/sessions/:id/pois", "Raised POI"}, {"POST /api/pois/:id/accept", "Accepted POI"},
        {"POST /api/pois/:id/reject", "Rejected POI"}, {"POST /api/pois/:id/withdraw", "Withdrew POI"},
        {"POST /api/sessions/:id/motions", "Proposed motion"}, {"POST /api/sessions/:id/motions/next", "Put motion to a vote"},
        {"POST /api/sessions/:id/motions/undo", "Undid chair action"}, {"POST /api/motions/:id/withdraw", "Withdrew motion"},
        {"POST /api/motions/:id/out-of-order", "Ruled motion out of order"},
        {"POST /api/sessions/:id/votes", "Opened vote"}, {"POST /api/votes/:id/ballot", "Cast ballot"},
        {"POST /api/votes/:id/record", "Recorded or corrected ballot"}, {"POST /api/votes/:id/close", "Closed vote"},
        {"PUT /api/scoring/criteria", "Changed scoring criteria"}, {"PUT /api/awards/config", "Changed award rules"},
        {"POST /api/committees/:id/marksheets", "Created marksheet"}, {"PUT /api/marksheets/:id", "Edited marks"},
        {"POST /api/marksheets/:id/submit", "Submitted marksheet"}, {"POST /api/marksheets/:id/approve", "Approved marksheet"},
        {"POST /api/marksheets/:id/return", "Returned marksheet"}, {"POST /api/marksheets/:id/publish", "Published marksheet"},
        {"POST /api/committees/:id/marksheets/publish", "Published committee results"},
        {"POST /api/backups", "Created backup"}, {"POST /api/backups/:id/restore", "Restored backup"},
        {"POST /api/persistence/verify", "Ran consistency check"},
        {"POST /api/announcements", "Posted announcement"}, {"DELETE /api/announcements/:id", "Deleted announcement"},
    };
    return m;
}
}  // namespace

AuditManager::AuditManager(Auth& auth) : auth_(auth) {}

std::string AuditManager::label(const std::string& method, const std::string& route) {
    auto it = labels().find(method + " " + route);
    return it != labels().end() ? it->second : method + " " + route;
}

std::string AuditManager::hashOf(const AuditRec& a) {
    std::string s = std::string(a.prevHash) + "|" + std::to_string(a.seq) + "|" + a.at + "|" + a.actor + "|" +
                    a.role + "|" + a.action + "|" + a.target + "|" + std::to_string(a.status);
    return Auth::toHex(Auth::sha256(s));
}

void AuditManager::record(const std::string& actor, const std::string& role, const std::string& action,
                          const std::string& target, int status) {
    AuditRec a{};
    a.seq = audit_next_seq();
    pers_copy_field(a.at, nowTimestamp().c_str(), DSA_STAMP_LEN);
    pers_copy_field(a.actor, actor.c_str(), DSA_ID_LEN);
    pers_copy_field(a.role, role.c_str(), sizeof a.role);
    pers_copy_field(a.action, action.c_str(), AUDIT_ACTION_LEN);
    pers_copy_field(a.target, target.c_str(), AUDIT_TARGET_LEN);
    a.status = status;
    AuditRec last;
    std::string prev = audit_get(audit_count() - 1, &last) == DSA_OK ? std::string(last.hash) : kGenesis;
    pers_copy_field(a.prevHash, prev.c_str(), AUDIT_HASH_LEN);
    pers_copy_field(a.hash, hashOf(a).c_str(), AUDIT_HASH_LEN);
    const Database& db = Database::instance();
    audit_append(&a, db.isOpen() ? db.filePath("audit_log.txt").c_str() : nullptr);
}

void AuditManager::attach(Router& router) {
    router.setObserver([this](const HttpRequest& req, const HttpResponse& res) {
        if (req.method == "GET") return;
        if (req.route == "/api/auth/login" || req.route == "/api/auth/logout") return;  // login history covers these
        std::string actor = "public", role = "public";
        int r = -1;
        if (auth_.identify(req, actor, r)) role = RoleManager::roleKey(r);
        else actor = "public";
        // Target: the route's parameters, or the new record's ID on creation.
        std::string target;
        for (const auto& p : req.params) target += (target.empty() ? "" : " ") + p.first + "=" + p.second;
        if (target.empty() && res.status == 201) {
            try {
                Json body = Json::parse(res.body);
                const Json& d = body["data"];
                std::string id = d["id"].isString() ? d["id"].asString()
                                 : d["member"]["id"].isString() ? d["member"]["id"].asString()
                                 : d["user"]["id"].isString() ? d["user"]["id"].asString() : "";
                if (!id.empty()) target = "new=" + id;
                if (actor == "public" && !id.empty()) actor = id;  // self-registration
            } catch (const JsonError&) {
            }
        }
        record(actor, role, label(req.method, req.route), target, res.status);
    });
}

Json AuditManager::list(const HttpRequest& q) const {
    std::string actor = q.queryParam("actor"), role = q.queryParam("role"), text = q.queryParam("q");
    bool failuresOnly = q.queryParam("result") == "failed";
    std::string lim = q.queryParam("limit", "200");
    int limit = lim.empty() || lim.find_first_not_of("0123456789") != std::string::npos ? 200 : std::stoi(lim.substr(0, 5));
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    Json arr = Json::array();
    for (int i = audit_count() - 1; i >= 0 && static_cast<int>(arr.size()) < limit; --i) {  // newest first
        AuditRec a;
        audit_get(i, &a);
        if (!actor.empty() && actor != a.actor) continue;
        if (!role.empty() && role != a.role) continue;
        if (failuresOnly && a.status < 400) continue;
        if (!text.empty()) {
            std::string hay = std::string(a.action) + " " + a.target;
            for (char& c : hay) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (hay.find(text) == std::string::npos) continue;
        }
        Json j = Json::object();
        j.set("seq", static_cast<double>(a.seq)).set("at", a.at).set("actor", a.actor).set("role", a.role)
            .set("action", a.action).set("target", a.target).set("status", a.status).set("hash", std::string(a.hash).substr(0, 12));
        arr.push(j);
    }
    Json out = Json::object();
    out.set("total", audit_count()).set("entries", arr);
    return out;
}

Json AuditManager::verify() const {
    std::string prev = kGenesis;
    int n = audit_count();
    for (int i = 0; i < n; ++i) {
        AuditRec a;
        audit_get(i, &a);
        if (prev != a.prevHash || hashOf(a) != a.hash) {
            Json out = Json::object();
            out.set("intact", false).set("checked", n).set("brokenAtSeq", static_cast<double>(a.seq))
                .set("message", "Entry " + std::to_string(a.seq) +
                                    " doesn't match the chain. It, or an entry before it, was changed or removed after it was written.");
            return out;
        }
        prev = a.hash;
    }
    Json out = Json::object();
    out.set("intact", true).set("checked", n).set("lastHash", prev.substr(0, 16))
        .set("message", "All " + std::to_string(n) + " entries match the hash chain.");
    return out;
}

std::string AuditManager::csv() const {
    auto cell = [](const std::string& v) {
        std::string out(v.size() * 2 + 3, '\0');
        int n = csv_escape(v.c_str(), &out[0], out.size());
        out.resize(n < 0 ? 0 : static_cast<std::size_t>(n));
        return out;
    };
    std::string s = "seq,at,actor,role,action,target,status,hash\r\n";
    for (int i = 0; i < audit_count(); ++i) {
        AuditRec a;
        audit_get(i, &a);
        s += std::to_string(a.seq) + "," + cell(a.at) + "," + cell(a.actor) + "," + cell(a.role) + "," +
             cell(a.action) + "," + cell(a.target) + "," + std::to_string(a.status) + "," + a.hash + "\r\n";
    }
    return s;
}

void registerAuditRoutes(Router& r, AuditManager& am, Auth& auth) {
    r.get("/api/audit", [&am, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(am.list(q)); });
    r.get("/api/audit/verify", [&am, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(am.verify()); });
    r.get("/api/audit/export", [&am, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        HttpResponse res;
        res.contentType = "text/csv; charset=utf-8";
        res.body = am.csv();
        res.headers.emplace_back("Content-Disposition", "attachment; filename=\"munsphere-audit-log.csv\"");
        return res; });
}

}  // namespace mun
