// MUNSphere — Module12_Persistence/PersistenceManager.h
// Backup, verified restore with automatic rollback, a save -> reload ->
// save round-trip check, and a cross-module integrity scan.
#pragma once

#include <string>

#include "API.h"
#include "AuditManager.h"
#include "Auth.h"

namespace mun {

class PersistenceManager {
public:
    explicit PersistenceManager(AuditManager& audit);

    Json status() const;                                         // every data file
    Json createBackup(const std::string& label);
    Json listBackups() const;
    // Validates, keeps a safety backup, rolls back on failure. Restores every
    // store except the audit log, which keeps its full history.
    Json restore(const std::string& backupId);
    Json verify();                                                // round trip + integrity
    Json integrity() const;

    std::string backupsDir() const;

private:
    AuditManager& audit_;
    static std::string fileSha(const std::string& path, long long* bytes);
    std::string newBackupId() const;
};

void registerPersistenceRoutes(Router& router, PersistenceManager& pm, Auth& auth);

}  // namespace mun
