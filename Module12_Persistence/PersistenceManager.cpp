// MUNSphere — Module12_Persistence/PersistenceManager.cpp
#include "PersistenceManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

#include "Database.h"
#include "RoleManager.h"

namespace fs = std::filesystem;

namespace mun {

namespace {

bool validBackupId(const std::string& id) {
    return !id.empty() && id.size() <= 32 && id.find_first_not_of("0123456789-") == std::string::npos;
}

// Records = non-empty lines after the header.
long long countRecords(const std::string& path, bool* headerOk) {
    std::ifstream in(path, std::ios::binary);
    std::string line;
    long long n = 0;
    *headerOk = false;
    if (!std::getline(in, line)) return 0;
    *headerOk = line.rfind("#MUNSPHERE|", 0) == 0;
    while (std::getline(in, line))
        if (!line.empty() && line != "\r") n++;
    return n;
}

// Files whose records live in a hash table: load order differs from save
// order, so the round trip compares the set of lines, not their order.
bool orderFree(const std::string& name) { return name == "accounts.txt" || name == "delegates.txt"; }

std::string canonical(const std::string& path, bool sortLines) {
    std::ifstream in(path, std::ios::binary);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    if (sortLines && lines.size() > 1) std::sort(lines.begin() + 1, lines.end());
    std::string all;
    for (const std::string& l : lines) all += l + "\n";
    return Auth::toHex(Auth::sha256(all));
}

void issue(Json& arr, const char* severity, const std::string& store, const std::string& record, const std::string& msg) {
    Json j = Json::object();
    j.set("severity", severity).set("store", store).set("record", record).set("message", msg);
    arr.push(j);
}

}  // namespace

PersistenceManager::PersistenceManager(AuditManager& audit) : audit_(audit) {}

std::string PersistenceManager::backupsDir() const { return Database::instance().filePath("backups"); }

std::string PersistenceManager::fileSha(const std::string& path, long long* bytes) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string data = ss.str();
    if (bytes) *bytes = static_cast<long long>(data.size());
    return Auth::toHex(Auth::sha256(data));
}

Json PersistenceManager::status() const {
    Json arr = Json::array();
    long long total = 0;
    for (const std::string& name : Database::fileNames()) {
        std::string p = Database::instance().filePath(name);
        Json j = Json::object();
        j.set("file", name);
        std::error_code ec;
        if (!fs::exists(p, ec)) {
            j.set("exists", false).set("bytes", 0).set("records", 0).set("valid", true)
                .set("note", "Not created yet: this store is empty.");
        } else {
            bool ok = false;
            long long recs = countRecords(p, &ok);
            long long bytes = static_cast<long long>(fs::file_size(p, ec));
            total += bytes;
            j.set("exists", true).set("bytes", static_cast<double>(bytes)).set("records", static_cast<double>(recs)).set("valid", ok);
        }
        arr.push(j);
    }
    Json out = Json::object();
    out.set("dataDir", Database::instance().dataDir()).set("files", arr).set("totalBytes", static_cast<double>(total))
        .set("skippedOnLoad", Database::instance().skippedLines());
    return out;
}

std::string PersistenceManager::newBackupId() const {
    std::string ts = nowTimestamp();  // YYYY-MM-DD HH:MM:SS
    std::string id = ts.substr(0, 10) + "-" + ts.substr(11, 2) + ts.substr(14, 2) + ts.substr(17, 2);
    std::string candidate = id;
    for (int n = 2; fs::exists(fs::path(backupsDir()) / candidate); ++n) candidate = id + "-" + std::to_string(n);
    return candidate;
}

Json PersistenceManager::createBackup(const std::string& rawLabel) {
    std::string label = validate::trim(rawLabel);
    if (label.size() > 100) throw ApiError(400, "VALIDATION", "Keep the backup label under 100 characters.");
    std::string id = newBackupId();
    fs::path dir = fs::path(backupsDir()) / id;
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) throw ApiError(500, "BACKUP_FAILED", "Couldn't create the backup folder.");

    // Every change is already on disk, so the data files ARE the current state.
    std::string manifestPath = (dir / "manifest.txt").string();
    char tmp[DSA_PATH_LEN + 8];
    FILE* mf = pers_begin_save(manifestPath.c_str(), "backup_manifest", tmp, sizeof tmp);
    if (!mf) throw ApiError(500, "BACKUP_FAILED", "Couldn't write the backup manifest.");
    fputs("label|", mf);
    pers_write_field(mf, label.c_str());
    fputs("\ncreated|", mf);
    pers_write_field(mf, nowTimestamp().c_str());
    fputc('\n', mf);
    int files = 0;
    long long total = 0;
    for (const std::string& name : Database::fileNames()) {
        std::string src = Database::instance().filePath(name);
        if (!fs::exists(src)) continue;
        std::string dst = (dir / name).string();
        if (pers_copy_file(src.c_str(), dst.c_str()) != DSA_OK) {   // C file handling, atomic per file
            fclose(mf);
            fs::remove_all(dir, ec);
            throw ApiError(500, "BACKUP_FAILED", "Copying " + name + " failed. No backup was kept.");
        }
        long long bytes = 0;
        std::string sha = fileSha(dst, &bytes);
        fputs("file|", mf);
        pers_write_field(mf, name.c_str());
        fputc('|', mf);
        fprintf(mf, "%lld|", bytes);
        pers_write_field(mf, sha.c_str());
        fputc('\n', mf);
        files++;
        total += bytes;
    }
    if (pers_finish_save(mf, tmp, manifestPath.c_str()) != DSA_OK) {
        fs::remove_all(dir, ec);
        throw ApiError(500, "BACKUP_FAILED", "Couldn't finish the backup manifest.");
    }
    Json out = Json::object();
    out.set("id", id).set("label", label).set("files", files).set("bytes", static_cast<double>(total));
    return out;
}

namespace {
struct Manifest {
    std::string label, created;
    std::vector<std::pair<std::string, std::pair<long long, std::string>>> files;  // name -> (bytes, sha)
};

bool readManifest(const std::string& path, Manifest& m) {
    FILE* f = nullptr;
    if (pers_open_for_load(path.c_str(), "backup_manifest", &f) != DSA_OK || !f) return false;
    char line[PERS_LINE_MAX];
    char* fld[PERS_MAX_FIELDS];
    int r;
    while ((r = pers_read_line(f, line, sizeof line)) != 0) {
        if (r < 0 || !line[0]) continue;
        int n = pers_split_fields(line, fld, PERS_MAX_FIELDS);
        if (n == 2 && std::string(fld[0]) == "label") m.label = fld[1];
        else if (n == 2 && std::string(fld[0]) == "created") m.created = fld[1];
        else if (n == 4 && std::string(fld[0]) == "file")
            m.files.push_back({fld[1], {std::atoll(fld[2]), fld[3]}});
    }
    fclose(f);
    return true;
}
}  // namespace

Json PersistenceManager::listBackups() const {
    Json arr = Json::array();
    std::error_code ec;
    if (!fs::is_directory(backupsDir(), ec)) return arr;
    std::vector<std::string> ids;
    for (const auto& e : fs::directory_iterator(backupsDir(), ec))
        if (e.is_directory() && validBackupId(e.path().filename().string())) ids.push_back(e.path().filename().string());
    std::sort(ids.rbegin(), ids.rend());  // newest first
    for (const std::string& id : ids) {
        Manifest m;
        Json j = Json::object();
        j.set("id", id);
        if (!readManifest((fs::path(backupsDir()) / id / "manifest.txt").string(), m)) {
            j.set("valid", false).set("label", "").set("created", "");
        } else {
            long long bytes = 0;
            for (const auto& f : m.files) bytes += f.second.first;
            j.set("valid", true).set("label", m.label).set("created", m.created)
                .set("files", static_cast<double>(m.files.size())).set("bytes", static_cast<double>(bytes));
        }
        arr.push(j);
    }
    return arr;
}

Json PersistenceManager::restore(const std::string& backupId) {
    if (!validBackupId(backupId)) throw ApiError(400, "VALIDATION", "That isn't a backup ID.");
    fs::path dir = fs::path(backupsDir()) / backupId;
    Manifest m;
    if (!fs::is_directory(dir) || !readManifest((dir / "manifest.txt").string(), m))
        throw ApiError(404, "NOT_FOUND", "No usable backup called " + backupId + ".");

    // 1. Check every file against the manifest before touching anything.
    std::set<std::string> known(Database::fileNames().begin(), Database::fileNames().end());
    for (const auto& f : m.files) {
        if (!known.count(f.first)) throw ApiError(409, "BACKUP_INVALID", "The backup lists an unknown file: " + f.first + ".");
        std::string p = (dir / f.first).string();
        long long bytes = -1;
        if (!fs::exists(p) || fileSha(p, &bytes) != f.second.second || bytes != f.second.first)
            throw ApiError(409, "BACKUP_INVALID", f.first + " in this backup has changed or is missing since it was made. Nothing was restored.");
        bool header = false;
        countRecords(p, &header);
        if (!header) throw ApiError(409, "BACKUP_INVALID", f.first + " in this backup isn't a MUNSphere file. Nothing was restored.");
    }

    // 2. Safety backup of the current state.
    Json safety = createBackup("Automatic, before restoring " + backupId);
    fs::path safetyDir = fs::path(backupsDir()) / safety["id"].asString();

    // The audit log is history, not state: restoring data must never rewrite
    // it, so it is kept as it is (the backup's copy stays in the backup).
    auto putBack = [&](const fs::path& from, const std::set<std::string>& present) {
        for (const std::string& name : Database::fileNames()) {
            if (name == "audit_log.txt") continue;
            std::string dst = Database::instance().filePath(name);
            if (present.count(name)) pers_copy_file((from / name).string().c_str(), dst.c_str());
            else { std::error_code ec; fs::remove(dst, ec); }  // file didn't exist at backup time
        }
    };
    std::set<std::string> inBackup, inSafety;
    for (const auto& f : m.files) inBackup.insert(f.first);
    for (const std::string& name : Database::fileNames())
        if (fs::exists(safetyDir / name)) inSafety.insert(name);

    // 3. Copy in and reload; on any failure, put the safety copy back.
    try {
        putBack(dir, inBackup);
        Database::instance().loadAllFrom(Database::instance().dataDir());
    } catch (const std::exception& e) {
        putBack(safetyDir, inSafety);
        Database::instance().loadAllFrom(Database::instance().dataDir());
        throw ApiError(500, "RESTORE_FAILED", std::string("Restoring failed and the previous data was put back: ") + e.what());
    }
    Json out = Json::object();
    out.set("restored", backupId).set("label", m.label).set("safetyBackup", safety["id"]);
    return out;
}

Json PersistenceManager::integrity() const {
    Json issues = Json::array();
    std::vector<CommitteeRec> coms(static_cast<std::size_t>(committee_count()));
    coms.resize(static_cast<std::size_t>(committee_to_array(coms.data(), static_cast<int>(coms.size()))));
    for (const CommitteeRec& c : coms)
        if (conference_find(c.conferenceId, nullptr, nullptr) != DSA_OK)
            issue(issues, "error", "committees", c.id, "Belongs to conference " + std::string(c.conferenceId) + ", which doesn't exist.");
    std::vector<SessionRec> ses(static_cast<std::size_t>(session_count()));
    ses.resize(static_cast<std::size_t>(session_to_array(ses.data(), static_cast<int>(ses.size()))));
    for (const SessionRec& s : ses)
        if (committee_find(s.committeeId, nullptr, nullptr) != DSA_OK)
            issue(issues, "error", "sessions", s.id, "Belongs to committee " + std::string(s.committeeId) + ", which doesn't exist.");
    std::vector<DelegateRec> dels(static_cast<std::size_t>(delegate_count()));
    dels.resize(static_cast<std::size_t>(delegate_to_array(dels.data(), static_cast<int>(dels.size()))));
    for (const DelegateRec& d : dels) {
        AccountRec a;
        if (account_find(d.id, &a, nullptr) != DSA_OK || a.role != ROLE_DELEGATE)
            issue(issues, "error", "delegates", d.id, "Has no matching delegate login account.");
        if (d.committeeId[0] && committee_find(d.committeeId, nullptr, nullptr) != DSA_OK)
            issue(issues, "error", "delegates", d.id, "Placed in committee " + std::string(d.committeeId) + ", which doesn't exist.");
        if (d.conferenceId[0] && conference_find(d.conferenceId, nullptr, nullptr) != DSA_OK)
            issue(issues, "warning", "delegates", d.id, "Registered for conference " + std::string(d.conferenceId) + ", which doesn't exist.");
        for (int k = 0; k < DELEGATE_PREFS; ++k)
            if (d.prefs[k][0] && committee_find(d.prefs[k], nullptr, nullptr) != DSA_OK)
                issue(issues, "warning", "delegates", d.id, "A committee preference (" + std::string(d.prefs[k]) + ") no longer exists.");
    }
    std::vector<AccountRec> accts(static_cast<std::size_t>(account_count()));
    accts.resize(static_cast<std::size_t>(account_to_array(accts.data(), static_cast<int>(accts.size()))));
    for (const AccountRec& a : accts) {
        if (a.role == ROLE_DELEGATE && delegate_find(a.id, nullptr, nullptr) != DSA_OK)
            issue(issues, "error", "accounts", a.id, "Delegate account without a delegate record.");
        if (a.role == ROLE_EB && eb_find(a.id, nullptr, nullptr) != DSA_OK)
            issue(issues, "warning", "accounts", a.id, "EB account without a committee assignment.");
    }
    std::vector<EbRec> ebs(static_cast<std::size_t>(eb_count()));
    ebs.resize(static_cast<std::size_t>(eb_to_array(ebs.data(), static_cast<int>(ebs.size()))));
    for (const EbRec& e : ebs)
        if (committee_find(e.committeeId, nullptr, nullptr) != DSA_OK)
            issue(issues, "error", "executive_board", e.id, "Assigned to committee " + std::string(e.committeeId) + ", which doesn't exist.");
    std::vector<MarksheetRec> sheets(static_cast<std::size_t>(sheet_count()) + 1);
    int ns = sheet_list("", -1, sheets.data(), static_cast<int>(sheets.size()));
    for (int i = 0; i < ns; ++i)
        if (delegate_find(sheets[static_cast<std::size_t>(i)].delegateId, nullptr, nullptr) != DSA_OK)
            issue(issues, "error", "marksheets", sheets[static_cast<std::size_t>(i)].id, "For a delegate who no longer exists.");
    Json out = Json::object();
    int errors = 0;
    for (const Json& x : issues.asArray()) if (x["severity"].asString() == "error") errors++;
    out.set("issues", issues).set("errors", errors).set("warnings", static_cast<double>(issues.size()) - errors);
    return out;
}

// Save everything to A, load it back from A, save again to B, compare.
// If save and load are true inverses, every file in B equals A.
Json PersistenceManager::verify() {
    Database& db = Database::instance();
    fs::path base = fs::path(db.dataDir()) / ".verify";
    std::error_code ec;
    fs::remove_all(base, ec);
    std::string a = (base / "a").string(), b = (base / "b").string();
    db.saveAllTo(a);
    db.loadAllFrom(a);       // memory now holds exactly what was in A
    db.saveAllTo(b);
    Json files = Json::array();
    bool allMatch = true;
    for (const std::string& name : Database::fileNames()) {
        std::string pa = (fs::path(a) / name).string(), pb = (fs::path(b) / name).string();
        bool sortLines = orderFree(name);
        std::string ha = canonical(pa, sortLines), hb = canonical(pb, sortLines);
        bool ok = false;
        long long recs = countRecords(pa, &ok);
        Json j = Json::object();
        j.set("file", name).set("records", static_cast<double>(recs)).set("match", ha == hb)
            .set("compare", sortLines ? "same records (hash-table order)" : "byte-identical").set("sha256", ha.substr(0, 16));
        if (ha != hb) allMatch = false;
        files.push(j);
    }
    db.saveAllTo(db.dataDir());   // keep the real folder exactly in step with memory
    fs::remove_all(base, ec);
    Json out = Json::object();
    out.set("roundTripOk", allMatch).set("files", files).set("integrity", integrity());
    return out;
}

void registerPersistenceRoutes(Router& r, PersistenceManager& pm, Auth& auth) {
    r.get("/api/persistence/status", [&pm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(pm.status()); });
    r.post("/api/persistence/verify", [&pm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(pm.verify()); });
    r.get("/api/backups", [&pm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(pm.listBackups()); });
    r.post("/api/backups", [&pm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        Json b = q.body.empty() ? Json::object() : q.json();
        return HttpResponse::ok(pm.createBackup(b["label"].isString() ? b["label"].asString() : ""), 201); });
    r.post("/api/backups/:id/restore", [&pm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(pm.restore(q.param("id"))); });
}

}  // namespace mun
