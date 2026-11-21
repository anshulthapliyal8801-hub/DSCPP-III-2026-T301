// MUNSphere — Shared/Database.cpp
#include "Database.h"

#include <filesystem>
#include <stdexcept>
#include <system_error>

#include "Common.h"
#include "dsa_api.h"

namespace fs = std::filesystem;

namespace mun {

namespace {
std::string in(const std::string& dir, const char* name) { return (fs::path(dir) / name).string(); }

void check(int rc, const std::string& what, const std::string& dir) {
    if (rc == DSA_OK) return;
    if (rc == DSA_ERR_INVALID)
        throw std::runtime_error("A " + what + " data file in '" + dir +
                                 "' is not a valid MUNSphere file. It was left untouched. "
                                 "Restore it from a backup or move it away, then start again.");
    throw std::runtime_error("Loading " + what + " data failed (code " + std::to_string(rc) + ").");
}
}  // namespace

const std::vector<std::string>& Database::fileNames() {
    static const std::vector<std::string> names = {
        "conferences.txt", "committees.txt", "sessions.txt", "accounts.txt", "login_history.txt",
        "executive_board.txt", "delegates.txt", "attendance.txt", "speaker_lists.txt", "speeches.txt",
        "speak_requests.txt", "pois.txt", "motions.txt", "motion_actions.txt", "votes.txt", "ballots.txt",
        "criteria.txt", "marksheets.txt", "marksheet_revisions.txt", "award_rules.txt",
        "announcements.txt", "audit_log.txt"};
    return names;
}

Database& Database::instance() {
    static Database db;
    return db;
}

std::string Database::filePath(const std::string& fileName) const {
    return (fs::path(dir_) / fileName).string();
}

void Database::loadAllFrom(const std::string& d) {
    if (in(d, "marksheet_revisions.txt").size() >= DSA_PATH_LEN)
        throw std::runtime_error("The data folder path is too long.");
    int skipped = 0, s1 = 0;
    check(m1_load(in(d, "conferences.txt").c_str(), in(d, "committees.txt").c_str(), in(d, "sessions.txt").c_str(), &s1),
          "conference", d);
    skipped += s1;
    check(accounts_load(in(d, "accounts.txt").c_str(), in(d, "login_history.txt").c_str(), &skipped), "account", d);
    check(eb_load(in(d, "executive_board.txt").c_str(), &skipped), "Executive Board", d);
    check(delegates_load(in(d, "delegates.txt").c_str(), &skipped), "delegate", d);
    check(att_load(in(d, "attendance.txt").c_str(), &skipped), "attendance", d);
    check(speakers_load(in(d, "speaker_lists.txt").c_str(), in(d, "speeches.txt").c_str(),
                        in(d, "speak_requests.txt").c_str(), in(d, "pois.txt").c_str(), &skipped),
          "speaker list", d);
    check(motions_load(in(d, "motions.txt").c_str(), in(d, "motion_actions.txt").c_str(), &skipped), "motion", d);
    check(votes_load(in(d, "votes.txt").c_str(), in(d, "ballots.txt").c_str(), &skipped), "voting", d);
    check(scoring_load(in(d, "criteria.txt").c_str(), in(d, "marksheets.txt").c_str(),
                       in(d, "marksheet_revisions.txt").c_str(), &skipped),
          "scoring", d);
    check(awards_load(in(d, "award_rules.txt").c_str(), &skipped), "award", d);
    check(ann_load(in(d, "announcements.txt").c_str(), &skipped), "announcement", d);
    check(audit_load(in(d, "audit_log.txt").c_str(), &skipped), "audit log", d);
    skipped_ = skipped;
}

void Database::open(const std::string& dataDir) {
    std::error_code ec;
    fs::create_directories(dataDir, ec);
    if (ec || !fs::is_directory(dataDir))
        throw std::runtime_error("Can't create or open the data folder '" + dataDir + "'.");
    loadAllFrom(dataDir);
    dir_ = dataDir;
    open_ = true;
}

int Database::saveStore(Store store, const std::string& d) const {
    switch (store) {
    case Store::Conference:
        return m1_save(in(d, "conferences.txt").c_str(), in(d, "committees.txt").c_str(), in(d, "sessions.txt").c_str());
    case Store::Accounts: return accounts_save(in(d, "accounts.txt").c_str(), in(d, "login_history.txt").c_str());
    case Store::ExecutiveBoard: return eb_save(in(d, "executive_board.txt").c_str());
    case Store::Delegates: return delegates_save(in(d, "delegates.txt").c_str());
    case Store::Attendance: return att_save(in(d, "attendance.txt").c_str());
    case Store::Speakers:
        return speakers_save(in(d, "speaker_lists.txt").c_str(), in(d, "speeches.txt").c_str(),
                             in(d, "speak_requests.txt").c_str(), in(d, "pois.txt").c_str());
    case Store::Motions: return motions_save(in(d, "motions.txt").c_str(), in(d, "motion_actions.txt").c_str());
    case Store::Votes: return votes_save(in(d, "votes.txt").c_str(), in(d, "ballots.txt").c_str());
    case Store::Scoring:
        return scoring_save(in(d, "criteria.txt").c_str(), in(d, "marksheets.txt").c_str(),
                            in(d, "marksheet_revisions.txt").c_str());
    case Store::Awards: return awards_save(in(d, "award_rules.txt").c_str());
    case Store::Announcements: return ann_save(in(d, "announcements.txt").c_str());
    case Store::Audit: return audit_save(in(d, "audit_log.txt").c_str());
    }
    return DSA_ERR_INVALID;
}

void Database::save(Store store) const {
    if (!open_) throw ApiError(500, "STORE_CLOSED", "The data store is not open.");
    if (saveStore(store, dir_) != DSA_OK)
        throw ApiError(500, "PERSIST_FAILED",
                       "The change was applied but couldn't be written to disk. "
                       "Check that the data folder is writable.");
}

void Database::saveAllTo(const std::string& dir) const {
    std::error_code ec;
    fs::create_directories(dir, ec);
    const Store all[] = {Store::Conference, Store::Accounts, Store::ExecutiveBoard, Store::Delegates,
                         Store::Attendance, Store::Speakers, Store::Motions, Store::Votes, Store::Scoring,
                         Store::Awards, Store::Announcements, Store::Audit};
    for (Store s : all)
        if (saveStore(s, dir) != DSA_OK)
            throw ApiError(500, "PERSIST_FAILED", "Writing the data files to '" + dir + "' failed.");
}

}  // namespace mun
