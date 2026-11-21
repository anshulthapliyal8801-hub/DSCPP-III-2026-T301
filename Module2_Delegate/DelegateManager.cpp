// MUNSphere — Module2_Delegate/DelegateManager.cpp
#include "DelegateManager.h"

#include <chrono>
#include <memory>
#include <set>
#include <vector>

#include "Conference.h"
#include "Database.h"
#include "RoleManager.h"

namespace mun {

namespace {
// A heap array of fixed-size ID strings, the shape the C API expects.
using IdArray = std::unique_ptr<char[][DSA_ID_LEN]>;
IdArray makeIds(int n) { return IdArray(new char[static_cast<std::size_t>(n) + 1][DSA_ID_LEN]); }
}  // namespace

DelegateManager::DelegateManager(Auth& auth, ConferenceManager& conferences)
    : auth_(auth), conferences_(conferences) {}

Delegate DelegateManager::get(const std::string& id) const {
    DelegateRec rec;
    if (delegate_find(id.c_str(), &rec, nullptr) != DSA_OK)
        throw ApiError(404, "NOT_FOUND", "No delegate with ID '" + id + "'.");
    return Delegate(auth_.getAccount(id), rec);
}

void DelegateManager::save(const Delegate& d, bool nameChanged) {
    DelegateRec rec = d.toRecord();
    if (delegate_update(&rec) != DSA_OK)  // re-indexes the BST if the name changed
        throw ApiError(500, "STORE", "The delegate record couldn't be updated.");
    if (nameChanged) auth_.saveAccount(d.account());
    Database::instance().save(Store::Delegates);
}

Json DelegateManager::registerDelegate(const Json& body) {
    std::string name = requireString(body, "name", 100);
    std::string email = Person::normalizeEmail(requireString(body, "email", 100));
    std::string institution = optionalString(body, "institution", 100, "");
    std::string password = body["password"].isString() ? body["password"].asString() : "";
    Auth::checkPasswordStrength(password);

    // Duplicate prevention: one delegate account per email address.
    std::vector<DelegateRec> all(static_cast<std::size_t>(delegate_count()));
    int n = delegate_to_array(all.data(), static_cast<int>(all.size()));
    for (int i = 0; i < n; ++i)
        if (email == all[static_cast<std::size_t>(i)].email)
            throw ApiError(409, "DUPLICATE_EMAIL", "A delegate account with this email already exists. "
                                                   "Log in, or ask the admin to reset your password.");

    User account = auth_.createAccount(ROLE_DELEGATE, name, password, false);
    DelegateRec rec{};
    pers_copy_field(rec.id, account.id().c_str(), DSA_ID_LEN);
    pers_copy_field(rec.name, account.name().c_str(), DSA_NAME_LEN);
    pers_copy_field(rec.email, email.c_str(), DSA_NAME_LEN);
    pers_copy_field(rec.institution, institution.c_str(), DSA_NAME_LEN);
    rec.status = REGISTRATION_NONE;
    if (delegate_insert(&rec) != DSA_OK) {
        account_delete(account.id().c_str());
        Database::instance().save(Store::Accounts);
        throw ApiError(500, "STORE", "The delegate record couldn't be stored.");
    }
    Database::instance().save(Store::Delegates);
    return auth_.login(account.id(), password);  // signed in straight away
}

Json DelegateManager::me(const AuthContext& ctx) const {
    Delegate d = get(ctx.accountId);
    Json j = d.toJson();
    if (!d.conferenceId().empty()) {
        try { j.set("conference", conferences_.getConference(d.conferenceId()).toJson()); }
        catch (const ApiError&) {}
    }
    if (d.assigned()) {
        try { j.set("committee", conferences_.getCommittee(d.committeeId()).toJson()); }
        catch (const ApiError&) {}
    }
    Json prefs = Json::array();
    for (const std::string& p : d.preferences()) {
        try { prefs.push(conferences_.getCommittee(p).toJson()); }
        catch (const ApiError&) {}
    }
    j.set("preferenceDetails", prefs);
    return j;
}

Json DelegateManager::updateProfile(const std::string& id, const Json& body) {
    Delegate d = get(id);
    bool nameChanged = false;
    if (body.has("name")) {
        std::string before = d.name();
        d.setName(requireString(body, "name", 100));
        nameChanged = before != d.name();
    }
    if (body.has("email")) {
        std::string email = Person::normalizeEmail(requireString(body, "email", 100));
        std::vector<DelegateRec> all(static_cast<std::size_t>(delegate_count()));
        int n = delegate_to_array(all.data(), static_cast<int>(all.size()));
        for (int i = 0; i < n; ++i)
            if (email == all[static_cast<std::size_t>(i)].email && id != all[static_cast<std::size_t>(i)].id)
                throw ApiError(409, "DUPLICATE_EMAIL", "Another delegate already uses this email.");
        d.setEmail(email);
    }
    if (body.has("institution")) d.setInstitution(optionalString(body, "institution", 100, ""));
    save(d, nameChanged);
    return d.toJson();
}

Json DelegateManager::apply(const AuthContext& ctx, const Json& body) {
    Delegate d = get(ctx.accountId);
    std::string confId = requireString(body, "conferenceId", DSA_ID_LEN - 1);
    conferences_.getConference(confId);  // 404 if missing
    const Json& raw = body["preferences"];
    if (!raw.isArray()) throw ApiError(400, "VALIDATION", "Preferences must be a list of committee IDs.");
    std::vector<std::string> prefs;
    std::set<std::string> seen;
    for (const Json& p : raw.asArray()) {
        if (!p.isString() || p.asString().empty()) continue;
        Committee c = conferences_.getCommittee(p.asString());
        if (c.conferenceId() != confId)
            throw ApiError(400, "VALIDATION", c.abbreviation() + " isn't part of this conference.");
        if (!seen.insert(c.id()).second)
            throw ApiError(400, "VALIDATION", "Each committee can only be chosen once.");
        prefs.push_back(c.id());
    }
    d.apply(confId, prefs);
    save(d, false);
    return me(ctx);
}

Json DelegateManager::withdraw(const AuthContext& ctx) {
    Delegate d = get(ctx.accountId);
    d.withdraw();
    save(d, false);
    return me(ctx);
}

Json DelegateManager::sessionsFor(const AuthContext& ctx) const {
    Delegate d = get(ctx.accountId);
    Json arr = Json::array();
    if (!d.assigned()) return arr;
    for (const Session& s : conferences_.listSessions(d.committeeId(), "")) arr.push(s.toJson());
    return arr;
}

Json DelegateManager::list(const HttpRequest& q) const {
    std::string status = q.queryParam("status"), conf = q.queryParam("conferenceId"),
                com = q.queryParam("committeeId"), text = q.queryParam("q");
    std::set<std::string> matches;
    if (!text.empty()) {  // linear search across name, ID, institution, portfolio
        std::vector<DelegateRec> found(static_cast<std::size_t>(delegate_count()));
        int cmp = 0;
        int n = delegate_search_text(text.c_str(), found.data(), static_cast<int>(found.size()), &cmp);
        for (int i = 0; i < n; ++i) matches.insert(found[static_cast<std::size_t>(i)].id);
    }
    // Alphabetical order comes straight from the BST's in-order traversal.
    int total = delegate_count();
    IdArray ids = makeIds(total);
    int n = bst_inorder(ids.get(), total + 1);
    Json arr = Json::array();
    for (int i = 0; i < n; ++i) {
        std::string id = ids[i];
        if (!text.empty() && !matches.count(id)) continue;
        Delegate d = get(id);
        if (!status.empty() && Delegate::statusKey(d.status()) != status &&
            !(status == "assigned" && d.assigned()) &&
            !(status == "unassigned" && d.status() == REGISTRATION_APPROVED && !d.assigned()))
            continue;
        if (!conf.empty() && d.conferenceId() != conf) continue;
        if (!com.empty() && d.committeeId() != com) continue;
        arr.push(d.toJson());
    }
    return arr;
}

Json DelegateManager::approve(const std::string& id) {
    Delegate d = get(id);
    d.approve();
    save(d, false);
    return d.toJson();
}

Json DelegateManager::reject(const std::string& id, const Json& body) {
    Delegate d = get(id);
    d.reject(body["reason"].isString() ? body["reason"].asString() : "");
    save(d, false);
    return d.toJson();
}

void DelegateManager::remove(const std::string& id) {
    Delegate d = get(id);
    if (d.assigned())
        throw ApiError(409, "HAS_DEPENDENTS", "Remove this delegate from their committee first.");
    delegate_delete(id.c_str());
    account_delete(id.c_str());
    auth_.revokeSessions(id);
    Database::instance().save(Store::Delegates);
    Database::instance().save(Store::Accounts);
}

// ------------------------------------------------------------ DSA lab

Json DelegateManager::hashState() const {
    Json buckets = Json::array();
    int nb = delegate_bucket_count(), longest = 0, used = 0;
    for (int b = 0; b < nb; ++b) {
        char ids[64][DSA_ID_LEN];
        int len = delegate_bucket_length(b);
        int k = delegate_bucket_ids(b, ids, 64);
        Json chain = Json::array();
        for (int i = 0; i < k; ++i) chain.push(ids[i]);
        Json j = Json::object();
        j.set("index", b).set("length", len).set("ids", chain);
        buckets.push(j);
        if (len > longest) longest = len;
        if (len > 0) used++;
    }
    Json out = Json::object();
    out.set("hashFunction", "djb2: h = h * 33 + c, start 5381; index = h mod buckets")
        .set("collisionHandling", "Separate chaining (linked list per bucket)")
        .set("size", delegate_count())
        .set("bucketCount", nb)
        .set("usedBuckets", used)
        .set("longestChain", longest)
        .set("loadFactor", delegate_load_factor())
        .set("maxLoadFactor", 0.75)
        .set("rehashes", delegate_rehash_count())
        .set("buckets", buckets);
    return out;
}

Json DelegateManager::bstState() const {
    int n = delegate_count();
    IdArray ids = makeIds(n);
    auto toArr = [&](int k) {
        Json a = Json::array();
        for (int i = 0; i < k; ++i) a.push(std::string(ids[i]));
        return a;
    };
    Json out = Json::object();
    out.set("inorder", toArr(bst_inorder(ids.get(), n + 1)));
    out.set("preorder", toArr(bst_preorder(ids.get(), n + 1)));
    out.set("postorder", toArr(bst_postorder(ids.get(), n + 1)));
    std::vector<int> parent(static_cast<std::size_t>(n) + 1), left(static_cast<std::size_t>(n) + 1);
    int k = bst_level_order(ids.get(), parent.data(), left.data(), n + 1);
    Json nodes = Json::array();
    for (int i = 0; i < k; ++i) {
        DelegateRec d;
        delegate_find(ids[i], &d, nullptr);
        Json j = Json::object();
        j.set("id", d.id).set("name", d.name).set("parent", parent[static_cast<std::size_t>(i)])
            .set("isLeft", left[static_cast<std::size_t>(i)] == 1);
        nodes.push(j);
    }
    char mn[DSA_ID_LEN] = "", mx[DSA_ID_LEN] = "";
    bst_min(mn);
    bst_max(mx);
    out.set("nodes", nodes)
        .set("height", bst_height())
        .set("nodeCount", bst_node_count())
        .set("min", mn)
        .set("max", mx)
        .set("key", "Delegate name (case-insensitive), then ID");
    return out;
}

Json DelegateManager::traceSearch(const std::string& method, const std::string& key) const {
    if (key.empty()) throw ApiError(400, "VALIDATION", "Enter something to search for.");
    SearchTrace t;
    DelegateRec out;
    int rc;
    std::string algorithm, complexity;
    auto t0 = std::chrono::steady_clock::now();
    if (method == "hash") {
        rc = delegate_search_hash(key.c_str(), &out, &t);
        algorithm = "Hash table lookup by ID";
        complexity = "Average O(1); worst O(n) if every key collides";
    } else if (method == "bst") {
        rc = delegate_search_bst_name(key.c_str(), &out, &t);
        algorithm = "BST search by name";
        complexity = "O(h): O(log n) when balanced, O(n) when skewed";
    } else if (method == "linear") {
        rc = delegate_search_linear_id(key.c_str(), &out, &t);
        algorithm = "Linear search by ID (unsorted array)";
        complexity = "O(n)";
    } else if (method == "binary") {
        rc = delegate_search_binary_id(key.c_str(), &out, &t);
        algorithm = "Binary search by ID (array sorted by ID first)";
        complexity = "O(log n) for the search; sorting first costs extra";
    } else {
        throw ApiError(400, "VALIDATION", "Method must be hash, bst, linear or binary.");
    }
    auto t1 = std::chrono::steady_clock::now();
    Json path = Json::array();
    for (int i = 0; i < t.pathLen; ++i) path.push(std::string(t.path[i]));
    Json j = Json::object();
    j.set("method", method)
        .set("algorithm", algorithm)
        .set("key", key)
        .set("found", rc == DSA_OK)
        .set("result", rc == DSA_OK ? Json(std::string(out.id) + " " + out.name) : Json())
        .set("comparisons", t.comparisons)
        .set("path", path)
        .set("pathTruncated", t.comparisons > t.pathLen)
        .set("n", delegate_count())
        .set("elapsedNs", std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count())
        .set("complexity", complexity);
    return j;
}

std::vector<DelegateRec> delegatesInCommittee(const std::string& committeeId) {
    std::vector<DelegateRec> all(static_cast<std::size_t>(delegate_count()));
    all.resize(static_cast<std::size_t>(delegate_to_array(all.data(), static_cast<int>(all.size()))));
    std::vector<DelegateRec> out;
    for (const DelegateRec& d : all)
        if (!committeeId.empty() && committeeId == d.committeeId) out.push_back(d);
    return out;
}

Json delegateBrief(const std::string& delegateId) {
    DelegateRec d;
    if (delegateId.empty() || delegate_find(delegateId.c_str(), &d, nullptr) != DSA_OK) return Json();
    Json j = Json::object();
    j.set("id", d.id).set("name", d.name).set("portfolio", d.portfolio);
    return j;
}

// ================================================================= routes

void registerDelegateRoutes(Router& r, DelegateManager& dm, Auth& auth) {
    r.post("/api/auth/register", [&dm](const HttpRequest& q) {
        return HttpResponse::ok(dm.registerDelegate(q.json()), 201);
    });

    // Delegate's own data
    r.get("/api/delegate/me", [&dm, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireDelegate(ctx);
        return HttpResponse::ok(dm.me(ctx));
    });
    r.put("/api/delegate/me", [&dm, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireDelegate(ctx);
        return HttpResponse::ok(dm.updateProfile(ctx.accountId, q.json()));
    });
    r.post("/api/delegate/me/registration", [&dm, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireDelegate(ctx);
        return HttpResponse::ok(dm.apply(ctx, q.json()));
    });
    r.del("/api/delegate/me/registration", [&dm, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireDelegate(ctx);
        return HttpResponse::ok(dm.withdraw(ctx));
    });
    r.get("/api/delegate/me/sessions", [&dm, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireDelegate(ctx);
        return HttpResponse::ok(dm.sessionsFor(ctx));
    });

    // Admin management
    r.get("/api/delegates", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(dm.list(q));
    });
    r.get("/api/delegates/:id", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(dm.get(q.param("id")).toJson());
    });
    r.put("/api/delegates/:id", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(dm.updateProfile(q.param("id"), q.json()));
    });
    r.post("/api/delegates/:id/approve", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(dm.approve(q.param("id")));
    });
    r.post("/api/delegates/:id/reject", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(dm.reject(q.param("id"), q.json()));
    });
    r.del("/api/delegates/:id", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        dm.remove(q.param("id"));
        Json j = Json::object();
        j.set("deleted", q.param("id"));
        return HttpResponse::ok(j);
    });

    // Module 2 lab
    r.get("/api/dsa/module2/hash", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(dm.hashState());
    });
    r.get("/api/dsa/module2/bst", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(dm.bstState());
    });
    r.get("/api/dsa/module2/search", [&dm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(dm.traceSearch(q.queryParam("method"), q.queryParam("key")));
    });
}

}  // namespace mun
