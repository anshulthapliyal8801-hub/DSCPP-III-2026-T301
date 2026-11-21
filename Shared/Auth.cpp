// MUNSphere — Shared/Auth.cpp
#include "Auth.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX   // stop <windows.h> defining min() and max() macros
#endif
#include <windows.h>
#include <bcrypt.h>   // BCryptGenRandom: link with -lbcrypt
#endif

#include "Database.h"
#include "dsa_api.h"

namespace mun {

// =============================================================== SHA-256
// FIPS 180-4. Kept as a small class so HMAC can copy a partially fed
// state (the key pads) instead of re-hashing them on every iteration.
namespace {

class Sha256 {
public:
    Sha256() { reset(); }
    void reset() {
        static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        std::memcpy(h_, init, sizeof h_);
        totalLen_ = 0;
        bufLen_ = 0;
    }
    void update(const unsigned char* data, std::size_t n) {
        totalLen_ += n;
        while (n > 0) {
            std::size_t take = std::min<std::size_t>(64 - bufLen_, n);
            std::memcpy(buf_ + bufLen_, data, take);
            bufLen_ += take;
            data += take;
            n -= take;
            if (bufLen_ == 64) { compress(buf_); bufLen_ = 0; }
        }
    }
    void update(const std::string& s) {
        update(reinterpret_cast<const unsigned char*>(s.data()), s.size());
    }
    std::string digest() {
        uint64_t bits = totalLen_ * 8;
        unsigned char pad = 0x80;
        update(&pad, 1);
        unsigned char zero = 0;
        while (bufLen_ != 56) update(&zero, 1);
        unsigned char len[8];
        for (int i = 0; i < 8; ++i) len[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        update(len, 8);
        std::string out(32, '\0');
        for (int i = 0; i < 8; ++i)
            for (int b = 0; b < 4; ++b)
                out[static_cast<std::size_t>(i * 4 + b)] = static_cast<char>(h_[i] >> (24 - 8 * b));
        return out;
    }

private:
    uint32_t h_[8];
    uint64_t totalLen_;
    unsigned char buf_[64];
    std::size_t bufLen_;

    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void compress(const unsigned char* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
            0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
            0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
            0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
            0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
            0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
            0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
            0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
                   (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6],
                 hh = h_[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
        h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += hh;
    }
};

// HMAC with the key pads absorbed once; each mac() copies two states.
class HmacSha256 {
public:
    explicit HmacSha256(std::string key) {
        if (key.size() > 64) key = Auth::sha256(key);
        key.resize(64, '\0');
        std::string ipad(64, '\0'), opad(64, '\0');
        for (int i = 0; i < 64; ++i) {
            ipad[static_cast<std::size_t>(i)] = static_cast<char>(key[static_cast<std::size_t>(i)] ^ 0x36);
            opad[static_cast<std::size_t>(i)] = static_cast<char>(key[static_cast<std::size_t>(i)] ^ 0x5c);
        }
        inner_.update(ipad);
        outer_.update(opad);
    }
    std::string mac(const std::string& msg) const {
        Sha256 in = inner_;
        in.update(msg);
        Sha256 out = outer_;
        out.update(in.digest());
        return out.digest();
    }

private:
    Sha256 inner_, outer_;
};

std::string fromHex(const std::string& hex) {
    if (hex.size() % 2) return {};
    std::string out;
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        int hi = val(hex[i]), lo = val(hex[i + 1]);
        if (hi < 0 || lo < 0) return {};
        out += static_cast<char>(hi * 16 + lo);
    }
    return out;
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

std::string Auth::sha256(const std::string& data) {
    Sha256 s;
    s.update(data);
    return s.digest();
}

std::string Auth::hmacSha256(const std::string& key, const std::string& msg) {
    return HmacSha256(key).mac(msg);
}

// PBKDF2 (RFC 8018): T_i = U_1 xor U_2 xor ... xor U_c,
// U_1 = HMAC(P, S || INT(i)), U_j = HMAC(P, U_{j-1})
std::string Auth::pbkdf2Sha256(const std::string& password, const std::string& salt,
                               int iterations, std::size_t length) {
    HmacSha256 prf(password);
    std::string out;
    for (uint32_t block = 1; out.size() < length; ++block) {
        std::string msg = salt;
        msg += static_cast<char>(block >> 24);
        msg += static_cast<char>(block >> 16);
        msg += static_cast<char>(block >> 8);
        msg += static_cast<char>(block);
        std::string u = prf.mac(msg), t = u;
        for (int j = 1; j < iterations; ++j) {
            u = prf.mac(u);
            for (std::size_t k = 0; k < t.size(); ++k) t[k] = static_cast<char>(t[k] ^ u[k]);
        }
        out += t;
    }
    out.resize(length);
    return out;
}

std::string Auth::toHex(const std::string& bytes) {
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) {
        out += d[c >> 4];
        out += d[c & 15];
    }
    return out;
}

std::string Auth::randomBytes(std::size_t n) {
    std::string out(n, '\0');
    if (n == 0) return out;
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&out[0]), static_cast<ULONG>(n),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        throw std::runtime_error("The system random number generator failed.");
#else
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom.read(&out[0], static_cast<std::streamsize>(n)))
        throw std::runtime_error("The system random number generator failed.");
#endif
    return out;
}

bool Auth::constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;  // no early exit, so timing doesn't reveal where they differ
}

// ======================================================== password storage

Auth::Auth() : Auth(Config{}) {}
Auth::Auth(Config cfg) : cfg_(cfg) {}

std::string Auth::hashPassword(const std::string& password) const {
    std::string salt = randomBytes(16);
    std::string hash = pbkdf2Sha256(password, salt, cfg_.iterations, 32);
    return "pbkdf2-sha256$" + std::to_string(cfg_.iterations) + "$" + toHex(salt) + "$" +
           toHex(hash);
}

bool Auth::verifyPassword(const std::string& password, const std::string& stored) {
    // Iterations are read from the stored string, so the work factor can
    // be raised later without breaking existing passwords.
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (;;) {
        std::size_t d = stored.find('$', start);
        parts.push_back(stored.substr(start, d == std::string::npos ? std::string::npos : d - start));
        if (d == std::string::npos) break;
        start = d + 1;
    }
    if (parts.size() != 4 || parts[0] != "pbkdf2-sha256") return false;
    if (parts[1].empty() || parts[1].size() > 8 ||
        parts[1].find_first_not_of("0123456789") != std::string::npos)
        return false;
    int iterations = std::stoi(parts[1]);
    std::string salt = fromHex(parts[2]);
    if (iterations < 1 || salt.empty() || parts[3].size() != 64) return false;
    std::string computed = toHex(pbkdf2Sha256(password, salt, iterations, 32));
    return constantTimeEquals(computed, parts[3]);
}

void Auth::checkPasswordStrength(const std::string& p) {
    if (p.size() < 8)
        throw ApiError(400, "WEAK_PASSWORD", "Password must be at least 8 characters.");
    if (p.size() > 128)
        throw ApiError(400, "WEAK_PASSWORD", "Password is too long (max 128 characters).");
    bool letter = false, digit = false;
    for (unsigned char c : p) {
        if (std::isalpha(c)) letter = true;
        if (std::isdigit(c)) digit = true;
    }
    if (!letter || !digit)
        throw ApiError(400, "WEAK_PASSWORD", "Password must contain at least one letter and one digit.");
}

std::string Auth::temporaryPassword() {
    // No 0/O, 1/l/I: temporary passwords get read aloud and copied by hand.
    static const std::string alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    const std::size_t limit = 256 - (256 % alphabet.size());  // avoid modulo bias
    for (;;) {
        std::string out;
        while (out.size() < 12) {
            for (unsigned char b : randomBytes(32)) {
                if (b >= limit) continue;  // rejection sampling
                out += alphabet[b % alphabet.size()];
                if (out.size() == 12) break;
            }
        }
        bool letter = false, digit = false;
        for (unsigned char c : out) {
            if (std::isalpha(c)) letter = true;
            if (std::isdigit(c)) digit = true;
        }
        if (letter && digit) return out;
    }
}

// =============================================================== accounts

User Auth::createAccount(int role, const std::string& name, const std::string& password,
                         bool mustChangePassword) {
    checkPasswordStrength(password);
    const char* prefix = User::idPrefix(role);
    char id[DSA_ID_LEN + 8];
    std::snprintf(id, sizeof id, "%s%03d", prefix, account_max_id_suffix(prefix) + 1);
    if (std::strlen(id) > DSA_ID_LEN - 1)
        throw ApiError(500, "ID_SPACE", "No more account IDs are available.");
    User u(id, role, name);
    u.setPassHash(hashPassword(password));
    u.setMustChangePassword(mustChangePassword);
    u.setCreatedAt(nowTimestamp());
    AccountRec rec = u.toRecord();
    int rc = account_insert(&rec);
    if (rc != DSA_OK) throw ApiError(500, "STORE", "The account couldn't be stored.");
    Database::instance().save(Store::Accounts);
    return u;
}

User Auth::getAccount(const std::string& id) const {
    AccountRec r;
    if (account_find(id.c_str(), &r, nullptr) != DSA_OK)
        throw ApiError(404, "NOT_FOUND", "No account with ID '" + id + "'.");
    return User::fromRecord(r);
}

void Auth::saveAccount(const User& u) {
    AccountRec r = u.toRecord();
    if (account_update(&r) != DSA_OK)
        throw ApiError(404, "NOT_FOUND", "No account with ID '" + u.id() + "'.");
    Database::instance().save(Store::Accounts);
}

// =============================================================== sessions

void Auth::recordLogin(const std::string& id, bool success, const std::string& detail) {
    LoginEventRec ev{};
    pers_copy_field(ev.accountId, id.c_str(), DSA_ID_LEN);
    pers_copy_field(ev.at, nowTimestamp().c_str(), DSA_STAMP_LEN);
    ev.success = success ? 1 : 0;
    pers_copy_field(ev.detail, detail.c_str(), sizeof ev.detail);
    login_history_add(&ev);
}

Json Auth::login(const std::string& rawId, const std::string& password) {
    std::string id = upper(validate::trim(rawId));
    if (id.empty() || password.empty())
        throw ApiError(400, "VALIDATION", "Enter your ID and password.");
    if (!validate::isId(id))
        throw ApiError(401, "BAD_CREDENTIALS", "ID or password is incorrect.");

    auto now = Clock::now();
    Failures& f = failures_[id];
    if (f.lockedUntil > now) {
        auto mins = std::chrono::duration_cast<std::chrono::minutes>(f.lockedUntil - now).count() + 1;
        throw ApiError(429, "LOCKED", "Too many failed attempts. Try again in " +
                                          std::to_string(mins) + " minute(s).");
    }

    AccountRec rec;
    bool exists = account_find(id.c_str(), &rec, nullptr) == DSA_OK;
    // Hash even for unknown IDs so response time doesn't reveal which IDs exist.
    static const std::string dummy = Auth(cfg_).hashPassword("dummy-password-1");
    bool ok = verifyPassword(password, exists ? std::string(rec.passHash) : dummy) && exists;

    if (!ok) {
        if (++f.count >= cfg_.maxFailures) {
            f.count = 0;
            f.lockedUntil = now + std::chrono::minutes(cfg_.lockMinutes);
        }
        if (exists) {
            recordLogin(id, false, "wrong password");
            Database::instance().save(Store::Accounts);
        }
        throw ApiError(401, "BAD_CREDENTIALS", "ID or password is incorrect.");
    }
    failures_.erase(id);
    if (!rec.active) {
        recordLogin(id, false, "account deactivated");
        Database::instance().save(Store::Accounts);
        throw ApiError(403, "DEACTIVATED", "This account is deactivated. Contact the conference admin.");
    }

    // Drop expired sessions while we're here.
    for (auto it = sessions_.begin(); it != sessions_.end();)
        it = it->second.expires <= now ? sessions_.erase(it) : std::next(it);

    std::string token = toHex(randomBytes(32));
    sessions_[token] = Session{id, now + std::chrono::hours(cfg_.tokenHours)};
    recordLogin(id, true, "signed in");
    Database::instance().save(Store::Accounts);

    HttpRequest fake;
    fake.headers["authorization"] = "Bearer " + token;
    AuthContext ctx = require(fake, true);
    Json out = Json::object();
    out.set("token", token).set("user", me(ctx));
    return out;
}

std::string Auth::bearerToken(const HttpRequest& req) {
    auto it = req.headers.find("authorization");
    if (it == req.headers.end()) return {};
    const std::string& v = it->second;
    if (v.size() < 8 || v.compare(0, 7, "Bearer ") != 0) return {};
    return v.substr(7);
}

void Auth::logout(const HttpRequest& req) {
    sessions_.erase(bearerToken(req));
}

AuthContext Auth::require(const HttpRequest& req, bool allowPending) {
    std::string token = bearerToken(req);
    if (token.empty()) throw ApiError(401, "AUTH_REQUIRED", "Log in to continue.");
    auto it = sessions_.find(token);
    auto now = Clock::now();
    if (it == sessions_.end() || it->second.expires <= now) {
        if (it != sessions_.end()) sessions_.erase(it);
        throw ApiError(401, "SESSION_EXPIRED", "Your session has ended. Log in again.");
    }
    // Re-read the account on every request: deactivation and role changes
    // take effect immediately, not when the token expires.
    AccountRec rec;
    if (account_find(it->second.accountId.c_str(), &rec, nullptr) != DSA_OK || !rec.active) {
        std::string id = it->second.accountId;
        revokeSessions(id);
        throw ApiError(401, "ACCOUNT_INACTIVE", "This account is no longer active.");
    }
    it->second.expires = now + std::chrono::hours(cfg_.tokenHours);  // sliding expiry

    AuthContext ctx;
    ctx.accountId = rec.id;
    ctx.role = rec.role;
    ctx.name = rec.name;
    ctx.mustChangePassword = rec.mustChangePassword != 0;
    if (rec.role == ROLE_EB) {
        EbRec eb;
        if (eb_find(rec.id, &eb, nullptr) == DSA_OK) {
            ctx.committeeId = eb.committeeId;
            ctx.ebRole = eb.ebRole;
            ctx.permissions = eb.permissions;
        }
    }
    if (ctx.mustChangePassword && !allowPending)
        throw ApiError(403, "PASSWORD_CHANGE_REQUIRED", "Set a new password before continuing.");
    return ctx;
}

bool Auth::identify(const HttpRequest& req, std::string& accountId, int& role) const {
    auto it = sessions_.find(bearerToken(req));
    if (it == sessions_.end() || it->second.expires <= Clock::now()) return false;
    AccountRec rec;
    if (account_find(it->second.accountId.c_str(), &rec, nullptr) != DSA_OK) return false;
    accountId = rec.id;
    role = rec.role;
    return true;
}

Json Auth::me(const AuthContext& ctx) const {
    Json j = Json::object();
    j.set("id", ctx.accountId)
        .set("name", ctx.name)
        .set("role", RoleManager::roleKey(ctx.role))
        .set("roleTitle", RoleManager::roleTitle(ctx.role))
        .set("mustChangePassword", ctx.mustChangePassword);
    if (ctx.isEb()) {
        Json eb = Json::object();
        eb.set("committeeId", ctx.committeeId)
            .set("ebRole", RoleManager::ebRoleName(ctx.ebRole))
            .set("permissions", RoleManager::permissionsToJson(ctx.permissions));
        CommitteeRec c;
        if (!ctx.committeeId.empty() && committee_find(ctx.committeeId.c_str(), &c, nullptr) == DSA_OK)
            eb.set("committeeName", c.name).set("committeeAbbreviation", c.abbreviation);
        j.set("eb", eb);
    }
    return j;
}

// ============================================================== passwords

void Auth::changePassword(const AuthContext& ctx, const std::string& current,
                          const std::string& next) {
    User u = getAccount(ctx.accountId);
    if (!verifyPassword(current, u.passHash()))
        throw ApiError(400, "WRONG_PASSWORD", "Your current password is incorrect.");
    if (current == next)
        throw ApiError(400, "WEAK_PASSWORD", "Choose a password different from the current one.");
    checkPasswordStrength(next);
    u.setPassHash(hashPassword(next));
    u.setMustChangePassword(false);
    u.setResetRequested(false);
    saveAccount(u);
}

std::string Auth::resetPassword(const std::string& accountId) {
    User u = getAccount(accountId);
    std::string temp = temporaryPassword();
    u.setPassHash(hashPassword(temp));
    u.setMustChangePassword(true);
    u.setResetRequested(false);
    saveAccount(u);
    revokeSessions(accountId);
    failures_.erase(accountId);
    return temp;
}

void Auth::requestReset(const std::string& rawId) {
    std::string id = upper(validate::trim(rawId));
    AccountRec r;
    if (!validate::isId(id) || account_find(id.c_str(), &r, nullptr) != DSA_OK) return;
    if (!r.active || r.resetRequested) return;
    r.resetRequested = 1;
    account_update(&r);
    Database::instance().save(Store::Accounts);
}

void Auth::setActive(const std::string& accountId, bool active) {
    User u = getAccount(accountId);
    u.setActive(active);
    saveAccount(u);
    if (!active) revokeSessions(accountId);
}

void Auth::revokeSessions(const std::string& accountId) {
    for (auto it = sessions_.begin(); it != sessions_.end();)
        it = it->second.accountId == accountId ? sessions_.erase(it) : std::next(it);
}

// ================================================================= routes

void registerAuthRoutes(Router& r, Auth& auth) {
    r.post("/api/auth/login", [&auth](const HttpRequest& q) {
        Json b = q.json();
        std::string id = b["id"].isString() ? b["id"].asString() : "";
        std::string pw = b["password"].isString() ? b["password"].asString() : "";
        return HttpResponse::ok(auth.login(id, pw));
    });
    r.post("/api/auth/logout", [&auth](const HttpRequest& q) {
        auth.logout(q);
        return HttpResponse::ok(Json::object());
    });
    r.get("/api/auth/me", [&auth](const HttpRequest& q) {
        return HttpResponse::ok(auth.me(auth.require(q, true)));
    });
    r.post("/api/auth/change-password", [&auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q, true);
        Json b = q.json();
        auth.changePassword(ctx, b["currentPassword"].isString() ? b["currentPassword"].asString() : "",
                            b["newPassword"].isString() ? b["newPassword"].asString() : "");
        return HttpResponse::ok(auth.me(auth.require(q, true)));
    });
    r.post("/api/auth/request-reset", [&auth](const HttpRequest& q) {
        Json b = q.json();
        auth.requestReset(b["id"].isString() ? b["id"].asString() : "");
        Json j = Json::object();
        j.set("message", "If that ID exists, the conference admin will see your request and "
                         "give you a temporary password.");
        return HttpResponse::ok(j);  // same answer whether or not the ID exists
    });
    r.get("/api/auth/history", [&auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        std::vector<LoginEventRec> ev(20);
        int n = login_history_to_array(ctx.accountId.c_str(), ev.data(), 20);
        Json arr = Json::array();
        for (int i = 0; i < n; ++i) {
            Json j = Json::object();
            j.set("at", ev[static_cast<std::size_t>(i)].at)
                .set("success", ev[static_cast<std::size_t>(i)].success == 1)
                .set("detail", ev[static_cast<std::size_t>(i)].detail);
            arr.push(j);
        }
        return HttpResponse::ok(arr);
    });

    // ---- Admin: accounts
    r.get("/api/admin/accounts", [&auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        std::string role = q.queryParam("role");
        std::vector<AccountRec> recs(static_cast<std::size_t>(account_count()));
        int n = account_to_array(recs.data(), static_cast<int>(recs.size()));
        recs.resize(static_cast<std::size_t>(n));
        std::sort(recs.begin(), recs.end(), [](const AccountRec& a, const AccountRec& b) {
            if (a.role != b.role) return a.role < b.role;
            return std::strcmp(a.id, b.id) < 0;
        });
        Json arr = Json::array();
        for (const AccountRec& a : recs)
            if (role.empty() || RoleManager::roleKey(a.role) == role)
                arr.push(User::fromRecord(a).toJson());
        return HttpResponse::ok(arr);
    });
    r.post("/api/admin/accounts/:id/deactivate", [&auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireAdmin(ctx);
        std::string id = q.param("id");
        if (id == ctx.accountId)
            throw ApiError(409, "SELF", "You can't deactivate your own account.");
        User u = auth.getAccount(id);
        if (u.role() == ROLE_ADMIN) {
            std::vector<AccountRec> recs(static_cast<std::size_t>(account_count()));
            int n = account_to_array(recs.data(), static_cast<int>(recs.size())), active = 0;
            for (int i = 0; i < n; ++i)
                if (recs[static_cast<std::size_t>(i)].role == ROLE_ADMIN && recs[static_cast<std::size_t>(i)].active) active++;
            if (active <= 1) throw ApiError(409, "LAST_ADMIN", "The last active admin can't be deactivated.");
        }
        auth.setActive(id, false);
        return HttpResponse::ok(auth.getAccount(id).toJson());
    });
    r.post("/api/admin/accounts/:id/activate", [&auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        auth.setActive(q.param("id"), true);
        return HttpResponse::ok(auth.getAccount(q.param("id")).toJson());
    });
    r.post("/api/admin/accounts/:id/reset-password", [&auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        std::string temp = auth.resetPassword(q.param("id"));
        Json j = Json::object();
        j.set("account", auth.getAccount(q.param("id")).toJson()).set("temporaryPassword", temp);
        return HttpResponse::ok(j);
    });
    r.get("/api/admin/login-history", [&auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        std::string id = q.queryParam("accountId");
        std::vector<LoginEventRec> ev(200);
        int n = login_history_to_array(id.c_str(), ev.data(), 200);
        Json arr = Json::array();
        for (int i = 0; i < n; ++i) {
            const LoginEventRec& e = ev[static_cast<std::size_t>(i)];
            Json j = Json::object();
            j.set("accountId", e.accountId).set("at", e.at).set("success", e.success == 1).set("detail", e.detail);
            arr.push(j);
        }
        return HttpResponse::ok(arr);
    });
}

}  // namespace mun
