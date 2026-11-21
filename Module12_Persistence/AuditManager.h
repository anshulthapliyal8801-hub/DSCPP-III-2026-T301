// MUNSphere — Module12_Persistence/AuditManager.h
// Records every change made through the API: who, as which role, what,
// to which record, and whether it succeeded. Request bodies are never
// stored, so passwords and ballot choices stay out of the log.
//
// Tamper evidence: each entry's hash is SHA-256 over the previous entry's
// hash and this entry's fields. Editing or deleting any past line breaks
// every hash after it, and verify() reports where.
#pragma once

#include <string>

#include "API.h"
#include "Auth.h"

namespace mun {

class AuditManager {
public:
    explicit AuditManager(Auth& auth);

    // Installs the router observer: one hook covers every module.
    void attach(Router& router);
    void record(const std::string& actor, const std::string& role, const std::string& action,
                const std::string& target, int status);

    Json list(const HttpRequest& q) const;   // newest first, filterable
    Json verify() const;                      // walks the hash chain
    std::string csv() const;

    static std::string label(const std::string& method, const std::string& route);
    static std::string hashOf(const AuditRec& a);

private:
    Auth& auth_;
};

void registerAuditRoutes(Router& router, AuditManager& am, Auth& auth);

}  // namespace mun
