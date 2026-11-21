// MUNSphere — Shared/Auth.h
// Authentication: password hashing (PBKDF2-HMAC-SHA256, implemented here
// with no external library), session tokens, login with lockout, password
// change and admin-driven password reset.
#pragma once

#include <chrono>
#include <map>
#include <string>

#include "API.h"
#include "RoleManager.h"
#include "User.h"

namespace mun {

class Auth {
public:
    struct Config {
        int iterations = 60000;  // PBKDF2 work factor (~tens of ms per login)
        int tokenHours = 8;      // a conference day
        int maxFailures = 5;     // failed logins before a lock
        int lockMinutes = 5;
    };

    Auth();
    explicit Auth(Config cfg);

    // ---- Cryptography (static so tests can check known vectors)
    static std::string sha256(const std::string& data);  // 32 raw bytes
    static std::string hmacSha256(const std::string& key, const std::string& msg);
    static std::string pbkdf2Sha256(const std::string& password, const std::string& salt,
                                    int iterations, std::size_t length);
    static std::string toHex(const std::string& bytes);
    static std::string randomBytes(std::size_t n);  // OS random generator
    static bool constantTimeEquals(const std::string& a, const std::string& b);

    // "pbkdf2-sha256$<iterations>$<salt hex>$<hash hex>"
    std::string hashPassword(const std::string& password) const;
    static bool verifyPassword(const std::string& password, const std::string& stored);
    static void checkPasswordStrength(const std::string& password);  // throws 400
    static std::string temporaryPassword();  // 12 easy-to-read characters

    // ---- Accounts
    // Creates and stores a new account (ID generated from the role prefix).
    User createAccount(int role, const std::string& name, const std::string& password,
                       bool mustChangePassword);
    User getAccount(const std::string& id) const;  // throws 404
    void saveAccount(const User& u);                // update + persist

    // ---- Sessions
    Json login(const std::string& id, const std::string& password);
    void logout(const HttpRequest& req);
    // Resolves the bearer token to the CURRENT account state. Throws 401
    // if missing/expired/deactivated, 403 while a password change is
    // required (unless allowPending).
    AuthContext require(const HttpRequest& req, bool allowPending = false);
    Json me(const AuthContext& ctx) const;
    // Who is calling, without side effects (no sliding expiry, no throw).
    bool identify(const HttpRequest& req, std::string& accountId, int& role) const;

    // ---- Passwords
    void changePassword(const AuthContext& ctx, const std::string& current,
                        const std::string& next);
    std::string resetPassword(const std::string& accountId);  // returns temp password
    void requestReset(const std::string& accountId);          // silent if unknown
    void setActive(const std::string& accountId, bool active);
    void revokeSessions(const std::string& accountId);
    std::size_t sessionCount() const { return sessions_.size(); }

    int iterations() const { return cfg_.iterations; }

private:
    using Clock = std::chrono::steady_clock;
    struct Session {
        std::string accountId;
        Clock::time_point expires;
    };
    struct Failures {
        int count = 0;
        Clock::time_point lockedUntil{};
    };

    Config cfg_;
    std::map<std::string, Session> sessions_;  // token -> session
    std::map<std::string, Failures> failures_;  // account id -> failures

    void recordLogin(const std::string& id, bool success, const std::string& detail);
    static std::string bearerToken(const HttpRequest& req);
};

// /api/auth/* and /api/admin/accounts* endpoints
void registerAuthRoutes(Router& router, Auth& auth);

}  // namespace mun
