// MUNSphere — Module1_Conference/Conference.cpp
#include "Conference.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "Database.h"
#include "RoleManager.h"

namespace mun {

// ================================================================ helpers

static void copyInto(char* dst, const std::string& src, std::size_t cap) {
    std::size_t n = src.size() < cap - 1 ? src.size() : cap - 1;
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

static std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Translates a C status code into the matching HTTP error.
[[noreturn]] static void throwDsa(int rc, const std::string& what, const std::string& id) {
    switch (rc) {
    case DSA_ERR_DUPLICATE:
        throw ApiError(409, "DUPLICATE_ID", what + " ID '" + id + "' is already in use.");
    case DSA_ERR_NOT_FOUND:
        throw ApiError(404, "NOT_FOUND", "No " + lower(what) + " with ID '" + id + "'.");
    case DSA_ERR_NO_MEMORY:
        throw ApiError(500, "NO_MEMORY", "The server ran out of memory.");
    default:
        throw ApiError(400, "INVALID", what + " data was rejected by the store.");
    }
}

static void rejectIdChange(const Json& body, const std::string& id) {
    if (body.has("id") && !(body["id"].isString() && body["id"].asString() == id))
        throw ApiError(400, "VALIDATION", "IDs can't be changed.");
}

static std::string today() { return nowTimestamp().substr(0, 10); }

// ============================================================ Conference

Conference::Conference(std::string id, std::string name, std::string venue,
                       std::string startDate, std::string endDate,
                       std::string description)
    : Entity(std::move(id)) {
    setName(name);
    setVenue(venue);
    setDates(startDate, endDate);
    setDescription(description);
}

Conference Conference::fromRecord(const ConferenceRec& r) {
    Conference c(r.id, "x", "x", "2000-01-01", "2000-01-01", "");
    c.name_ = r.name;
    c.venue_ = r.venue;
    c.startDate_ = r.startDate;
    c.endDate_ = r.endDate;
    c.description_ = r.description;
    return c;
}

ConferenceRec Conference::toRecord() const {
    ConferenceRec r{};
    copyInto(r.id, id_, DSA_ID_LEN);
    copyInto(r.name, name_, DSA_NAME_LEN);
    copyInto(r.venue, venue_, DSA_NAME_LEN);
    copyInto(r.startDate, startDate_, DSA_DATE_LEN);
    copyInto(r.endDate, endDate_, DSA_DATE_LEN);
    copyInto(r.description, description_, DSA_TEXT_LEN);
    return r;
}

void Conference::setName(const std::string& v) {
    std::string s = validate::trim(v);
    if (s.empty()) throw ApiError(400, "VALIDATION", "Conference name can't be empty.");
    if (s.size() > DSA_NAME_LEN - 1)
        throw ApiError(400, "VALIDATION", "Conference name is too long (max 100 characters).");
    name_ = s;
}

void Conference::setVenue(const std::string& v) {
    std::string s = validate::trim(v);
    if (s.empty()) throw ApiError(400, "VALIDATION", "Venue can't be empty.");
    if (s.size() > DSA_NAME_LEN - 1)
        throw ApiError(400, "VALIDATION", "Venue is too long (max 100 characters).");
    venue_ = s;
}

void Conference::setDates(const std::string& start, const std::string& end) {
    if (!validate::isDate(start))
        throw ApiError(400, "VALIDATION", "Start date must be a real date in YYYY-MM-DD form.");
    if (!validate::isDate(end))
        throw ApiError(400, "VALIDATION", "End date must be a real date in YYYY-MM-DD form.");
    if (end < start)
        throw ApiError(400, "VALIDATION", "End date can't be before the start date.");
    startDate_ = start;
    endDate_ = end;
}

void Conference::setDescription(const std::string& v) {
    std::string s = validate::trim(v);
    if (s.size() > DSA_TEXT_LEN - 1)
        throw ApiError(400, "VALIDATION", "Description is too long (max 300 characters).");
    description_ = s;
}

Json Conference::toJson() const {
    Json j = Json::object();
    j.set("id", id_)
        .set("name", name_)
        .set("venue", venue_)
        .set("startDate", startDate_)
        .set("endDate", endDate_)
        .set("description", description_);
    return j;
}

// =============================================================== Session

Session::Session(std::string id, std::string committeeId, std::string title,
                 std::string date, std::string startTime, std::string endTime,
                 SessionStatus status)
    : Entity(std::move(id)), committeeId_(std::move(committeeId)), status_(status) {
    setTitle(title);
    setSchedule(date, startTime, endTime);
}

Session Session::fromRecord(const SessionRec& r) {
    Session s(r.id, r.committeeId, "x", "2000-01-01", "00:00", "00:01", SESSION_SCHEDULED);
    s.title_ = r.title;
    s.date_ = r.date;
    s.startTime_ = r.startTime;
    s.endTime_ = r.endTime;
    s.status_ = static_cast<SessionStatus>(r.status);
    return s;
}

SessionRec Session::toRecord() const {
    SessionRec r{};
    copyInto(r.id, id_, DSA_ID_LEN);
    copyInto(r.committeeId, committeeId_, DSA_ID_LEN);
    copyInto(r.title, title_, DSA_NAME_LEN);
    copyInto(r.date, date_, DSA_DATE_LEN);
    copyInto(r.startTime, startTime_, DSA_TIME_LEN);
    copyInto(r.endTime, endTime_, DSA_TIME_LEN);
    r.status = status_;
    return r;
}

bool Session::overlaps(const Session& o) const {
    return committeeId_ == o.committeeId_ && date_ == o.date_ &&
           startTime_ < o.endTime_ && o.startTime_ < endTime_;
}

void Session::setTitle(const std::string& v) {
    std::string s = validate::trim(v);
    if (s.empty()) throw ApiError(400, "VALIDATION", "Session title can't be empty.");
    if (s.size() > DSA_NAME_LEN - 1)
        throw ApiError(400, "VALIDATION", "Session title is too long (max 100 characters).");
    title_ = s;
}

void Session::setSchedule(const std::string& date, const std::string& start,
                          const std::string& end) {
    if (!validate::isDate(date))
        throw ApiError(400, "VALIDATION", "Session date must be a real date in YYYY-MM-DD form.");
    if (!validate::isTime(start) || !validate::isTime(end))
        throw ApiError(400, "VALIDATION", "Times must be in 24-hour HH:MM form.");
    if (end <= start)
        throw ApiError(400, "VALIDATION", "End time must be after the start time.");
    date_ = date;
    startTime_ = start;
    endTime_ = end;
}

void Session::activate() {
    if (status_ != SESSION_SCHEDULED)
        throw ApiError(409, "INVALID_STATE", "Only a scheduled session can be activated.");
    status_ = SESSION_ACTIVE;
}

void Session::deactivate() {
    if (status_ != SESSION_ACTIVE)
        throw ApiError(409, "INVALID_STATE", "Only an active session can be deactivated.");
    status_ = SESSION_SCHEDULED;
}

void Session::complete() {
    if (status_ != SESSION_ACTIVE)
        throw ApiError(409, "INVALID_STATE", "Only an active session can be completed.");
    status_ = SESSION_COMPLETED;
}

Json Session::toJson() const {
    Json j = Json::object();
    j.set("id", id_)
        .set("committeeId", committeeId_)
        .set("title", title_)
        .set("date", date_)
        .set("startTime", startTime_)
        .set("endTime", endTime_)
        .set("status", lower(session_status_name(status_)))
        .set("statusLabel", session_status_name(status_));
    return j;
}

// ===================================================== ConferenceManager

std::string ConferenceManager::pickId(const Json& body, const char* prefix,
                                      int entity) const {
    if (body.has("id") && !body["id"].isNull()) {
        std::string id = requireString(body, "id", DSA_ID_LEN - 1);
        if (!validate::isId(id))
            throw ApiError(400, "VALIDATION",
                           "IDs use 2 to 15 capital letters, digits, '_' or '-'.");
        return id;
    }
    char buf[DSA_ID_LEN + 16];
    std::snprintf(buf, sizeof buf, "%s%03d", prefix, m1_max_id_suffix(entity, prefix) + 1);
    if (std::strlen(buf) > DSA_ID_LEN - 1)
        throw ApiError(500, "ID_SPACE", "No more IDs are available for this record type.");
    return buf;
}

// ---- Conferences -------------------------------------------------------

std::vector<Conference> ConferenceManager::listConferences() const {
    std::vector<ConferenceRec> recs(static_cast<std::size_t>(conference_count()));
    int n = conference_to_array(recs.data(), static_cast<int>(recs.size()));
    std::vector<Conference> out;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) out.push_back(Conference::fromRecord(recs[static_cast<std::size_t>(i)]));
    return out;
}

Conference ConferenceManager::getConference(const std::string& id) const {
    ConferenceRec r;
    int rc = conference_find(id.c_str(), &r, nullptr);
    if (rc != DSA_OK) throwDsa(rc, "Conference", id);
    return Conference::fromRecord(r);
}

Conference ConferenceManager::createConference(const Json& body) {
    std::string id = pickId(body, "CONF", M1_CONFERENCE);
    Conference c(id, requireString(body, "name", 100), requireString(body, "venue", 100),
                 requireString(body, "startDate", 10), requireString(body, "endDate", 10),
                 optionalString(body, "description", 300, ""));
    ConferenceRec rec = c.toRecord();
    int rc = conference_insert(&rec);
    if (rc != DSA_OK) throwDsa(rc, "Conference", id);
    Database::instance().saveModule1();
    return c;
}

Conference ConferenceManager::updateConference(const std::string& id, const Json& body) {
    Conference c = getConference(id);
    rejectIdChange(body, id);
    if (body.has("name")) c.setName(requireString(body, "name", 100));
    if (body.has("venue")) c.setVenue(requireString(body, "venue", 100));
    if (body.has("description")) c.setDescription(optionalString(body, "description", 300, ""));
    std::string start = body.has("startDate") ? requireString(body, "startDate", 10) : c.startDate();
    std::string end = body.has("endDate") ? requireString(body, "endDate", 10) : c.endDate();
    c.setDates(start, end);

    // New dates must still contain every session of this conference.
    for (const Committee& com : listCommittees(id, "")) {
        for (const Session& s : listSessions(com.id(), "")) {
            if (!c.coversDate(s.date()))
                throw ApiError(409, "SESSION_OUTSIDE_DATES",
                               "Session " + s.id() + " on " + s.date() +
                                   " would fall outside the new dates. Move or delete it first.");
        }
    }
    ConferenceRec rec = c.toRecord();
    int rc = conference_update(&rec);
    if (rc != DSA_OK) throwDsa(rc, "Conference", id);
    Database::instance().saveModule1();
    return c;
}

void ConferenceManager::deleteConference(const std::string& id) {
    getConference(id);  // 404 if missing
    int n = committee_count_by_conference(id.c_str());
    if (n > 0)
        throw ApiError(409, "HAS_DEPENDENTS",
                       "This conference still has " + std::to_string(n) +
                           " committee(s). Delete them first.");
    int rc = conference_delete(id.c_str());
    if (rc != DSA_OK) throwDsa(rc, "Conference", id);
    Database::instance().saveModule1();
}

// ---- Committees --------------------------------------------------------

std::vector<Committee> ConferenceManager::listCommittees(const std::string& conferenceId,
                                                         const std::string& query) const {
    std::vector<CommitteeRec> recs(static_cast<std::size_t>(committee_count()));
    int n;
    if (!query.empty()) {
        int cmp = 0;  // C linear search over the committee list
        n = committee_search_name(query.c_str(), recs.data(),
                                  static_cast<int>(recs.size()), &cmp);
    } else {
        n = committee_to_array(recs.data(), static_cast<int>(recs.size()));
    }
    std::vector<Committee> out;
    for (int i = 0; i < n; ++i) {
        const CommitteeRec& r = recs[static_cast<std::size_t>(i)];
        if (conferenceId.empty() || conferenceId == r.conferenceId)
            out.push_back(Committee::fromRecord(r));
    }
    return out;
}

Committee ConferenceManager::getCommittee(const std::string& id) const {
    CommitteeRec r;
    int rc = committee_find(id.c_str(), &r, nullptr);
    if (rc != DSA_OK) throwDsa(rc, "Committee", id);
    return Committee::fromRecord(r);
}

Committee ConferenceManager::createCommittee(const Json& body) {
    std::string id = pickId(body, "COM", M1_COMMITTEE);
    std::string confId = requireString(body, "conferenceId", DSA_ID_LEN - 1);
    if (conference_find(confId.c_str(), nullptr, nullptr) != DSA_OK)
        throw ApiError(400, "INVALID_REFERENCE", "Conference '" + confId + "' doesn't exist.");
    Committee c(id, confId, requireString(body, "name", 100),
                requireString(body, "abbreviation", 20),
                optionalString(body, "agenda", 300, ""),
                requireInt(body, "capacity", Committee::kMinCapacity, Committee::kMaxCapacity));
    for (const Committee& other : listCommittees(confId, ""))
        if (lower(other.abbreviation()) == lower(c.abbreviation()))
            throw ApiError(409, "DUPLICATE_NAME", "This conference already has a committee called " +
                                                      other.abbreviation() + ".");
    CommitteeRec rec = c.toRecord();
    int rc = committee_insert(&rec);
    if (rc != DSA_OK) throwDsa(rc, "Committee", id);
    Database::instance().saveModule1();
    return c;
}

Committee ConferenceManager::updateCommittee(const std::string& id, const Json& body) {
    Committee c = getCommittee(id);
    rejectIdChange(body, id);
    if (body.has("conferenceId") &&
        !(body["conferenceId"].isString() && body["conferenceId"].asString() == c.conferenceId()))
        throw ApiError(400, "VALIDATION", "A committee can't move to another conference.");
    if (body.has("name")) c.setName(requireString(body, "name", 100));
    if (body.has("abbreviation")) c.setAbbreviation(requireString(body, "abbreviation", 20));
    if (body.has("agenda")) c.setAgenda(optionalString(body, "agenda", 300, ""));
    if (body.has("capacity")) {
        int cap = requireInt(body, "capacity", Committee::kMinCapacity, Committee::kMaxCapacity);
        for (const auto& check : capacityChecks_) {
            std::string reason = check(id, cap);
            if (!reason.empty()) throw ApiError(409, "CAPACITY", reason);
        }
        c.setCapacity(cap);
    }
    for (const Committee& other : listCommittees(c.conferenceId(), ""))
        if (other.id() != id && lower(other.abbreviation()) == lower(c.abbreviation()))
            throw ApiError(409, "DUPLICATE_NAME", "This conference already has a committee called " +
                                                      other.abbreviation() + ".");
    CommitteeRec rec = c.toRecord();
    int rc = committee_update(&rec);
    if (rc != DSA_OK) throwDsa(rc, "Committee", id);
    Database::instance().saveModule1();
    return c;
}

void ConferenceManager::addCommitteeDependencyCheck(DependencyCheck check) {
    committeeChecks_.push_back(std::move(check));
}

void ConferenceManager::addCapacityCheck(CapacityCheck check) {
    capacityChecks_.push_back(std::move(check));
}

void ConferenceManager::deleteCommittee(const std::string& id) {
    getCommittee(id);
    int n = session_count_by_committee(id.c_str());
    if (n > 0)
        throw ApiError(409, "HAS_DEPENDENTS", "This committee still has " + std::to_string(n) +
                                                  " session(s). Delete them first.");
    for (const auto& check : committeeChecks_) {
        std::string reason = check(id);
        if (!reason.empty()) throw ApiError(409, "HAS_DEPENDENTS", reason);
    }
    int rc = committee_delete(id.c_str());
    if (rc != DSA_OK) throwDsa(rc, "Committee", id);
    Database::instance().saveModule1();
}

// ---- Sessions ----------------------------------------------------------

std::vector<Session> ConferenceManager::listSessions(const std::string& committeeId,
                                                     const std::string& status) const {
    int wanted = -1;
    if (!status.empty()) {
        std::string s = lower(status);
        for (int k = 0; k < SESSION_STATUS_COUNT; ++k)
            if (s == lower(session_status_name(k))) wanted = k;
        if (wanted < 0)
            throw ApiError(400, "VALIDATION", "Status must be scheduled, active or completed.");
    }
    std::vector<SessionRec> recs(static_cast<std::size_t>(session_count()));
    int n = session_to_array(recs.data(), static_cast<int>(recs.size()));
    std::vector<Session> out;
    for (int i = 0; i < n; ++i) {
        const SessionRec& r = recs[static_cast<std::size_t>(i)];
        if (!committeeId.empty() && committeeId != r.committeeId) continue;
        if (wanted >= 0 && r.status != wanted) continue;
        out.push_back(Session::fromRecord(r));
    }
    return out;
}

Session ConferenceManager::getSession(const std::string& id) const {
    SessionRec r;
    int rc = session_find(id.c_str(), &r, nullptr);
    if (rc != DSA_OK) throwDsa(rc, "Session", id);
    return Session::fromRecord(r);
}

void ConferenceManager::checkSessionRules(const Session& s, const std::string& ignoreId) const {
    CommitteeRec com;
    if (committee_find(s.committeeId().c_str(), &com, nullptr) != DSA_OK)
        throw ApiError(400, "INVALID_REFERENCE", "Committee '" + s.committeeId() + "' doesn't exist.");
    Conference conf = getConference(com.conferenceId);
    if (!conf.coversDate(s.date()))
        throw ApiError(400, "VALIDATION", "The session date must fall within the conference (" +
                                              conf.startDate() + " to " + conf.endDate() + ").");
    for (const Session& other : listSessions(s.committeeId(), "")) {
        if (other.id() != ignoreId && s.overlaps(other))
            throw ApiError(409, "TIME_CONFLICT",
                           "This time overlaps " + other.id() + " (" + other.startTime() + " to " +
                               other.endTime() + " on " + other.date() + ").");
    }
}

Session ConferenceManager::createSession(const Json& body) {
    std::string id = pickId(body, "SES", M1_SESSION);
    Session s(id, requireString(body, "committeeId", DSA_ID_LEN - 1),
              requireString(body, "title", 100), requireString(body, "date", 10),
              requireString(body, "startTime", 5), requireString(body, "endTime", 5),
              SESSION_SCHEDULED);
    checkSessionRules(s, "");
    SessionRec rec = s.toRecord();
    int rc = session_insert(&rec);
    if (rc != DSA_OK) throwDsa(rc, "Session", id);
    Database::instance().saveModule1();
    return s;
}

Session ConferenceManager::updateSession(const std::string& id, const Json& body) {
    Session s = getSession(id);
    rejectIdChange(body, id);
    if (s.status() != SESSION_SCHEDULED)
        throw ApiError(409, "INVALID_STATE", "Only scheduled sessions can be edited.");
    if (body.has("committeeId") &&
        !(body["committeeId"].isString() && body["committeeId"].asString() == s.committeeId()))
        throw ApiError(400, "VALIDATION", "A session can't move to another committee.");
    if (body.has("title")) s.setTitle(requireString(body, "title", 100));
    s.setSchedule(body.has("date") ? requireString(body, "date", 10) : s.date(),
                  body.has("startTime") ? requireString(body, "startTime", 5) : s.startTime(),
                  body.has("endTime") ? requireString(body, "endTime", 5) : s.endTime());
    checkSessionRules(s, id);
    return saveSessionState(s);
}

void ConferenceManager::deleteSession(const std::string& id) {
    Session s = getSession(id);
    if (s.status() != SESSION_SCHEDULED)
        throw ApiError(409, "INVALID_STATE",
                       "Active and completed sessions are kept as records and can't be deleted.");
    int rc = session_delete(id.c_str());
    if (rc != DSA_OK) throwDsa(rc, "Session", id);
    Database::instance().saveModule1();
}

Session ConferenceManager::saveSessionState(Session s) {
    SessionRec rec = s.toRecord();
    int rc = session_update(&rec);
    if (rc != DSA_OK) throwDsa(rc, "Session", s.id());
    Database::instance().saveModule1();
    return s;
}

Session ConferenceManager::activateSession(const std::string& id) {
    Session s = getSession(id);
    for (const Session& other : listSessions(s.committeeId(), "active"))
        if (other.id() != id)
            throw ApiError(409, "ALREADY_ACTIVE",
                           other.id() + " is already active in this committee. "
                                        "Deactivate or complete it first.");
    s.activate();
    return saveSessionState(s);
}

Session ConferenceManager::deactivateSession(const std::string& id) {
    Session s = getSession(id);
    s.deactivate();
    return saveSessionState(s);
}

Session ConferenceManager::completeSession(const std::string& id) {
    Session s = getSession(id);
    s.complete();
    return saveSessionState(s);
}

// ---- Overview ----------------------------------------------------------

Json ConferenceManager::overview() const {
    std::vector<Session> all = listSessions("", "");
    std::vector<Committee> coms = listCommittees("", "");
    auto committeeLabel = [&](const std::string& cid) -> Json {
        for (const Committee& c : coms)
            if (c.id() == cid) {
                Json j = Json::object();
                j.set("id", c.id()).set("abbreviation", c.abbreviation()).set("name", c.name());
                return j;
            }
        return Json();
    };
    Json active = Json::array(), upcoming = Json::array();
    std::string now = today();
    for (const Session& s : all) {  // already sorted by date and time (C list)
        if (s.status() == SESSION_ACTIVE) {
            Json j = s.toJson();
            j.set("committee", committeeLabel(s.committeeId()));
            active.push(j);
        } else if (s.status() == SESSION_SCHEDULED && s.date() >= now && upcoming.size() < 5) {
            Json j = s.toJson();
            j.set("committee", committeeLabel(s.committeeId()));
            upcoming.push(j);
        }
    }
    Json counts = Json::object();
    counts.set("conferences", conference_count())
        .set("committees", committee_count())
        .set("sessions", session_count())
        .set("activeSessions", active.size());
    Json out = Json::object();
    out.set("counts", counts).set("activeSessions", active).set("upcomingSessions", upcoming);
    return out;
}

Json ConferenceManager::liveSessions() const {
    Json arr = Json::array();
    for (const Session& s : listSessions("", "active")) {
        Json j = Json::object();
        j.set("id", s.id()).set("title", s.title()).set("date", s.date())
            .set("startTime", s.startTime()).set("endTime", s.endTime());
        CommitteeRec c;
        if (committee_find(s.committeeId().c_str(), &c, nullptr) == DSA_OK)
            j.set("committee", Json::object().set("abbreviation", c.abbreviation).set("name", c.name));
        arr.push(j);
    }
    return arr;
}

// ---- DSA traces --------------------------------------------------------

Json ConferenceManager::listStructure() const {
    Json conf = Json::array(), com = Json::array(), ses = Json::array();
    for (const Conference& c : listConferences()) {
        Json j = Json::object();
        j.set("id", c.id()).set("label", c.name());
        conf.push(j);
    }
    for (const Committee& c : listCommittees("", "")) {
        Json j = Json::object();
        j.set("id", c.id()).set("label", c.abbreviation());
        com.push(j);
    }
    for (const Session& s : listSessions("", "")) {
        Json j = Json::object();
        j.set("id", s.id()).set("label", s.date() + " " + s.startTime());
        ses.push(j);
    }
    Json out = Json::object();
    out.set("conferences", conf).set("committees", com).set("sessions", ses);
    return out;
}

Json ConferenceManager::traceIdSearch(const std::string& type, const std::string& id) const {
    if (id.empty()) throw ApiError(400, "VALIDATION", "Enter an ID to search for.");
    int cmp = 0, rc = 0;
    std::vector<std::string> order;
    auto t0 = std::chrono::steady_clock::now();
    if (type == "conference") rc = conference_find(id.c_str(), nullptr, &cmp);
    else if (type == "committee") rc = committee_find(id.c_str(), nullptr, &cmp);
    else if (type == "session") rc = session_find(id.c_str(), nullptr, &cmp);
    else throw ApiError(400, "VALIDATION", "Type must be conference, committee or session.");
    auto t1 = std::chrono::steady_clock::now();

    // A linear search visits nodes in list order, so the path is exactly
    // the first `cmp` nodes of the list.
    Json structure = listStructure();
    const Json& nodes = structure[type + "s"];
    Json path = Json::array();
    for (std::size_t i = 0; i < nodes.size() && static_cast<int>(i) < cmp; ++i)
        path.push(nodes.asArray()[i]["id"]);

    Json out = Json::object();
    out.set("algorithm", "Linear search (singly linked list)")
        .set("type", type)
        .set("target", id)
        .set("found", rc == DSA_OK)
        .set("comparisons", cmp)
        .set("listLength", nodes.size())
        .set("path", path)
        .set("elapsedNs",
             std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count())
        .set("complexity", "Best O(1), average and worst O(n)");
    return out;
}

Json ConferenceManager::traceNameSearch(const std::string& query) const {
    std::vector<CommitteeRec> recs(static_cast<std::size_t>(committee_count()));
    int cmp = 0;
    auto t0 = std::chrono::steady_clock::now();
    int n = committee_search_name(query.c_str(), recs.data(), static_cast<int>(recs.size()), &cmp);
    auto t1 = std::chrono::steady_clock::now();
    Json matches = Json::array();
    for (int i = 0; i < n; ++i) matches.push(Committee::fromRecord(recs[static_cast<std::size_t>(i)]).toJson());
    Json out = Json::object();
    out.set("algorithm", "Linear search, case-insensitive substring")
        .set("query", query)
        .set("comparisons", cmp)
        .set("matches", matches)
        .set("elapsedNs",
             std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count())
        .set("complexity", "O(n x m): every node is examined");
    return out;
}

// ================================================================ Routes

template <typename T>
static Json toJsonArray(const std::vector<T>& items) {
    Json arr = Json::array();
    for (const T& item : items) arr.push(item.toJson());  // virtual call
    return arr;
}

static Json deletedJson(const std::string& id) {
    Json j = Json::object();
    j.set("deleted", id);
    return j;
}

void registerModule1Routes(Router& r, ConferenceManager& m, Auth& auth) {
    // Public: the landing page shows what's in session (titles only).
    r.get("/api/public/live", [&m](const HttpRequest&) { return HttpResponse::ok(m.liveSessions()); });

    r.get("/api/overview", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(m.overview());
    });

    // Conferences: any signed-in user can read (delegates pick one);
    // only Admin changes them.
    r.get("/api/conferences", [&m, &auth](const HttpRequest& q) {
        auth.require(q);
        return HttpResponse::ok(toJsonArray(m.listConferences()));
    });
    r.post("/api/conferences", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(m.createConference(q.json()).toJson(), 201);
    });
    r.get("/api/conferences/:id", [&m, &auth](const HttpRequest& q) {
        auth.require(q);
        return HttpResponse::ok(m.getConference(q.param("id")).toJson());
    });
    r.put("/api/conferences/:id", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(m.updateConference(q.param("id"), q.json()).toJson());
    });
    r.del("/api/conferences/:id", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        m.deleteConference(q.param("id"));
        return HttpResponse::ok(deletedJson(q.param("id")));
    });

    // Committees: readable by any signed-in user (needed for preferences)
    r.get("/api/committees", [&m, &auth](const HttpRequest& q) {
        auth.require(q);
        return HttpResponse::ok(
            toJsonArray(m.listCommittees(q.queryParam("conferenceId"), q.queryParam("q"))));
    });
    r.post("/api/committees", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(m.createCommittee(q.json()).toJson(), 201);
    });
    r.get("/api/committees/:id", [&m, &auth](const HttpRequest& q) {
        auth.require(q);
        return HttpResponse::ok(m.getCommittee(q.param("id")).toJson());
    });
    r.put("/api/committees/:id", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(m.updateCommittee(q.param("id"), q.json()).toJson());
    });
    r.del("/api/committees/:id", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        m.deleteCommittee(q.param("id"));
        return HttpResponse::ok(deletedJson(q.param("id")));
    });

    // Sessions: Admin sees all; EB only their committee; delegates only
    // their assigned committee (resolved by DelegateManager's endpoint).
    r.get("/api/sessions", [&m, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        std::string committeeId = q.queryParam("committeeId");
        if (ctx.isDelegate())
            throw ApiError(403, "FORBIDDEN", "Delegates see their sessions on their own dashboard.");
        if (ctx.isEb()) {
            if (!committeeId.empty() && committeeId != ctx.committeeId)
                throw ApiError(403, "FORBIDDEN", "You can only view your assigned committee.");
            committeeId = ctx.committeeId;
            if (committeeId.empty()) return HttpResponse::ok(Json::array());
        }
        return HttpResponse::ok(toJsonArray(m.listSessions(committeeId, q.queryParam("status"))));
    });
    r.post("/api/sessions", [&m, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        Json body = q.json();
        std::string committeeId = body["committeeId"].isString() ? body["committeeId"].asString() : "";
        RoleManager::requireCommittee(ctx, committeeId, perm::MANAGE_SESSIONS);
        return HttpResponse::ok(m.createSession(body).toJson(), 201);
    });
    r.get("/api/sessions/:id", [&m, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        Session s = m.getSession(q.param("id"));
        RoleManager::requireCommittee(ctx, s.committeeId(), perm::VIEW);
        return HttpResponse::ok(s.toJson());
    });
    r.put("/api/sessions/:id", [&m, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireCommittee(ctx, m.getSession(q.param("id")).committeeId(), perm::MANAGE_SESSIONS);
        return HttpResponse::ok(m.updateSession(q.param("id"), q.json()).toJson());
    });
    r.del("/api/sessions/:id", [&m, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireCommittee(ctx, m.getSession(q.param("id")).committeeId(), perm::MANAGE_SESSIONS);
        m.deleteSession(q.param("id"));
        return HttpResponse::ok(deletedJson(q.param("id")));
    });
    r.post("/api/sessions/:id/activate", [&m, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireCommittee(ctx, m.getSession(q.param("id")).committeeId(), perm::RUN_SESSIONS);
        return HttpResponse::ok(m.activateSession(q.param("id")).toJson());
    });
    r.post("/api/sessions/:id/deactivate", [&m, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireCommittee(ctx, m.getSession(q.param("id")).committeeId(), perm::RUN_SESSIONS);
        return HttpResponse::ok(m.deactivateSession(q.param("id")).toJson());
    });
    r.post("/api/sessions/:id/complete", [&m, &auth](const HttpRequest& q) {
        AuthContext ctx = auth.require(q);
        RoleManager::requireCommittee(ctx, m.getSession(q.param("id")).committeeId(), perm::RUN_SESSIONS);
        return HttpResponse::ok(m.completeSession(q.param("id")).toJson());
    });

    // DSA demonstrations: Admin only
    r.get("/api/dsa/module1/structure", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(m.listStructure());
    });
    r.get("/api/dsa/module1/search", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(m.traceIdSearch(q.queryParam("type"), q.queryParam("id")));
    });
    r.get("/api/dsa/module1/search-name", [&m, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(m.traceNameSearch(q.queryParam("q")));
    });
}

}  // namespace mun
