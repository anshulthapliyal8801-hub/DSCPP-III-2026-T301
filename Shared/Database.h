// MUNSphere — Shared/Database.h
// The gateway to the ONE shared data store.
//
// The records themselves live in the C data structures (one store per
// module, never duplicated). Database owns the data folder and is the only
// class that tells the C layer to load or save, so every module persists
// the same way, to the same place, in the same format.
//
// It is a singleton: one data store per running server; the private
// constructor makes a second instance impossible.
#pragma once

#include <string>
#include <vector>

namespace mun {

enum class Store {
    Conference,      // Module 1: conferences, committees, sessions
    Accounts,        // Shared: accounts, login history
    ExecutiveBoard,  // Module 10: EB assignments
    Delegates,       // Module 2: delegate records
    Attendance,      // Module 4
    Speakers,        // Module 5: speaker lists, speeches, requests, POIs
    Motions,         // Module 6: motions and the undo stacks
    Votes,           // Module 7: votes and ballots
    Scoring,         // Module 8: criteria, marksheets, revision history
    Awards,          // Module 9: award rules and tie-break
    Announcements,   // Module 11
    Audit            // Module 12: full rewrite (normally appended line by line)
};

class Database {
public:
    static Database& instance();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Creates the folder if needed and loads every store from it. Throws
    // std::runtime_error with a readable message on failure, leaving any
    // damaged file untouched.
    void open(const std::string& dataDir);
    bool isOpen() const { return open_; }
    const std::string& dataDir() const { return dir_; }
    std::string filePath(const std::string& fileName) const;

    // Writes one store to the data folder. Throws ApiError(500) on failure.
    void save(Store store) const;
    void saveModule1() const { save(Store::Conference); }

    // Every data file the application keeps, in load order.
    static const std::vector<std::string>& fileNames();
    // Write every store into `dir` / load every store from `dir`
    // (used by backup, restore and the round-trip consistency check).
    void saveAllTo(const std::string& dir) const;
    void loadAllFrom(const std::string& dir);

    int skippedLines() const { return skipped_; }

private:
    Database() = default;
    std::string dir_;
    bool open_ = false;
    int skipped_ = 0;
    int saveStore(Store store, const std::string& dir) const;
};

}  // namespace mun
