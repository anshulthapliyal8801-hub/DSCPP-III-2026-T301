// MUNSphere — Module10_ExecutiveBoard/EBManager.cpp
#include "EBManager.h"

#include <algorithm>
#include <vector>

#include "Conference.h"
#include "Database.h"
#include "RoleManager.h"

namespace mun {

EBManager::EBManager(Auth& auth, ConferenceManager& conferences) : auth_(auth) {
    // A committee with EB members assigned can't be deleted.
    conferences.addCommitteeDependencyCheck([](const std::string& committeeId) -> std::string {
        int n = eb_count_by_committee(committeeId.c_str());
        return n > 0 ? "This committee still has " + std::to_string(n) +
                           " Executive Board member(s). Transfer them first."
                     : std::string();
    });
}

void EBManager::requireCommittee(const std::string& committeeId) const {
    if (committee_find(committeeId.c_str(), nullptr, nullptr) != DSA_OK)
        throw ApiError(400, "INVALID_REFERENCE", "Committee '" + committeeId + "' doesn't exist.");
}

std::unique_ptr<EBMember> EBManager::get(const std::string& id) const {
    EbRec rec;
    if (eb_find(id.c_str(), &rec, nullptr) != DSA_OK)
        throw ApiError(404, "NOT_FOUND", "No Executive Board member with ID '" + id + "'.");
    return EBMember::create(auth_.getAccount(id), rec);
}

Json EBManager::list(const std::string& committeeId) const {
    std::vector<EbRec> recs(static_cast<std::size_t>(eb_count()));
    int n = committeeId.empty()
                ? eb_to_array(recs.data(), static_cast<int>(recs.size()))
                : eb_list_by_committee(committeeId.c_str(), recs.data(), static_cast<int>(recs.size()));
    recs.resize(static_cast<std::size_t>(n));
    std::sort(recs.begin(), recs.end(),
              [](const EbRec& a, const EbRec& b) { return std::string(a.id) < b.id; });
    Json arr = Json::array();
    for (const EbRec& r : recs) {
        // Polymorphism: each object is a Chairperson, Moderator or Scribe,
        // and toJson()/roleTitle() resolve to that subclass at run time.
        std::unique_ptr<EBMember> m = EBMember::create(auth_.getAccount(r.id), r);
        arr.push(m->toJson());
    }
    return arr;
}

void EBManager::store(const EBMember& m, bool isNew) {
    EbRec rec = m.toRecord();
    int rc = isNew ? eb_insert(&rec) : eb_update(&rec);
    if (rc != DSA_OK) throw ApiError(500, "STORE", "The EB assignment couldn't be stored.");
    Database::instance().save(Store::ExecutiveBoard);
}

Json EBManager::create(const Json& body) {
    std::string name = requireString(body, "name", 100);
    std::string email = Person::normalizeEmail(requireString(body, "email", 100));
    int role = RoleManager::parseEbRole(requireString(body, "ebRole", 20));
    std::string committeeId = requireString(body, "committeeId", DSA_ID_LEN - 1);
    requireCommittee(committeeId);
    User probe("PROBE", ROLE_EB, name);  // validates the name before anything is stored
    (void)probe;

    std::string temp = Auth::temporaryPassword();
    User account = auth_.createAccount(ROLE_EB, name, temp, true);
    EbRec rec{};
    pers_copy_field(rec.id, account.id().c_str(), DSA_ID_LEN);
    pers_copy_field(rec.committeeId, committeeId.c_str(), DSA_ID_LEN);
    pers_copy_field(rec.email, email.c_str(), DSA_NAME_LEN);
    rec.ebRole = role;
    std::unique_ptr<EBMember> m = EBMember::create(account, rec);
    m->setPermissions(body.has("permissions") ? RoleManager::parsePermissions(body["permissions"])
                                              : m->defaultPermissions());
    try {
        store(*m, true);
    } catch (...) {
        account_delete(account.id().c_str());  // keep account and assignment in step
        Database::instance().save(Store::Accounts);
        throw;
    }
    Json out = Json::object();
    out.set("member", m->toJson()).set("temporaryPassword", temp);
    return out;
}

Json EBManager::update(const std::string& id, const Json& body) {
    std::unique_ptr<EBMember> m = get(id);
    User account = m->account();
    if (body.has("name")) {
        account.setName(requireString(body, "name", 100));
        auth_.saveAccount(account);
    }
    EbRec rec = m->toRecord();
    if (body.has("email")) pers_copy_field(rec.email, Person::normalizeEmail(requireString(body, "email", 100)).c_str(), DSA_NAME_LEN);
    bool roleChanged = false;
    if (body.has("ebRole")) {
        int role = RoleManager::parseEbRole(requireString(body, "ebRole", 20));
        roleChanged = role != rec.ebRole;
        rec.ebRole = role;
    }
    if (body.has("committeeId")) {  // transfer
        std::string committeeId = requireString(body, "committeeId", DSA_ID_LEN - 1);
        requireCommittee(committeeId);
        pers_copy_field(rec.committeeId, committeeId.c_str(), DSA_ID_LEN);
    }
    std::unique_ptr<EBMember> updated = EBMember::create(account, rec);
    if (body.has("permissions"))
        updated->setPermissions(RoleManager::parsePermissions(body["permissions"]));
    else if (roleChanged)
        updated->setPermissions(updated->defaultPermissions());  // new role, new defaults
    store(*updated, false);
    return updated->toJson();
}

Json EBManager::setAccess(const std::string& id, bool active) {
    get(id);  // 404 if not an EB member
    auth_.setActive(id, active);  // deactivation also ends their sessions
    return get(id)->toJson();
}

Json EBManager::permissionInfo() const {
    Json defaults = Json::object();
    for (int r = 0; r < EB_ROLE_COUNT; ++r)
        defaults.set(RoleManager::ebRoleName(r),
                     RoleManager::permissionsToJson(EBMember::defaultPermissionsFor(r)));
    Json out = Json::object();
    out.set("permissions", RoleManager::permissionCatalog()).set("defaults", defaults);
    return out;
}

void registerEBRoutes(Router& r, EBManager& eb, Auth& auth) {
    r.get("/api/eb", [&eb, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(eb.list(q.queryParam("committeeId")));
    });
    r.get("/api/eb/permissions", [&eb, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(eb.permissionInfo());
    });
    r.get("/api/eb/me", [&eb, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireEb(ctx);
        return HttpResponse::ok(eb.get(ctx.accountId)->toJson());
    });
    r.post("/api/eb", [&eb, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(eb.create(q.json()), 201);
    });
    r.get("/api/eb/:id", [&eb, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(eb.get(q.param("id"))->toJson());
    });
    r.put("/api/eb/:id", [&eb, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(eb.update(q.param("id"), q.json()));
    });
    r.post("/api/eb/:id/revoke", [&eb, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(eb.setAccess(q.param("id"), false));
    });
    r.post("/api/eb/:id/restore", [&eb, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(eb.setAccess(q.param("id"), true));
    });
}

}  // namespace mun
