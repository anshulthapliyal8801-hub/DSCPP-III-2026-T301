// MUNSphere — Module1_Conference/Conference.h
// Module 1 OOP layer: Conference and Session entities, and the
// ConferenceManager service that applies business rules before calling
// the C linked lists in conference_dsa.c.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "API.h"
#include "Auth.h"
#include "Committee.h"
#include "Common.h"
#include "dsa_api.h"

namespace mun {

// ------------------------------------------------------------ Conference
class Conference : public Entity {
public:
    Conference(std::string id, std::string name, std::string venue,
               std::string startDate, std::string endDate, std::string description);

    static Conference fromRecord(const ConferenceRec& r);
    ConferenceRec toRecord() const;

    const std::string& name() const { return name_; }
    const std::string& venue() const { return venue_; }
    const std::string& startDate() const { return startDate_; }
    const std::string& endDate() const { return endDate_; }
    const std::string& description() const { return description_; }
    bool coversDate(const std::string& date) const {
        return date >= startDate_ && date <= endDate_;  // ISO dates compare as text
    }

    void setName(const std::string& v);
    void setVenue(const std::string& v);
    void setDates(const std::string& start, const std::string& end);
    void setDescription(const std::string& v);

    Json toJson() const override;
    std::string entityName() const override { return "Conference"; }

private:
    std::string name_, venue_, startDate_, endDate_, description_;
};

// --------------------------------------------------------------- Session
class Session : public Entity {
public:
    Session(std::string id, std::string committeeId, std::string title,
            std::string date, std::string startTime, std::string endTime,
            SessionStatus status);

    static Session fromRecord(const SessionRec& r);
    SessionRec toRecord() const;

    const std::string& committeeId() const { return committeeId_; }
    const std::string& title() const { return title_; }
    const std::string& date() const { return date_; }
    const std::string& startTime() const { return startTime_; }
    const std::string& endTime() const { return endTime_; }
    SessionStatus status() const { return status_; }

    bool overlaps(const Session& other) const;

    void setTitle(const std::string& v);
    void setSchedule(const std::string& date, const std::string& start,
                     const std::string& end);

    // State machine: Scheduled -> Active -> Completed, and Active -> Scheduled.
    void activate();
    void deactivate();
    void complete();

    Json toJson() const override;
    std::string entityName() const override { return "Session"; }

private:
    std::string committeeId_, title_, date_, startTime_, endTime_;
    SessionStatus status_;
};

// ------------------------------------------------------ ConferenceManager
class ConferenceManager {
public:
    // A check returns "" if the committee may be deleted, otherwise the
    // reason it can't. Later modules (delegates, EB) register their own.
    using DependencyCheck = std::function<std::string(const std::string& committeeId)>;
    // Returns "" if the new capacity is acceptable, otherwise the reason.
    using CapacityCheck = std::function<std::string(const std::string& committeeId, int capacity)>;

    // Conferences
    std::vector<Conference> listConferences() const;
    Conference getConference(const std::string& id) const;
    Conference createConference(const Json& body);
    Conference updateConference(const std::string& id, const Json& body);
    void deleteConference(const std::string& id);

    // Committees
    std::vector<Committee> listCommittees(const std::string& conferenceId,
                                          const std::string& query) const;
    Committee getCommittee(const std::string& id) const;
    Committee createCommittee(const Json& body);
    Committee updateCommittee(const std::string& id, const Json& body);
    void deleteCommittee(const std::string& id);
    void addCommitteeDependencyCheck(DependencyCheck check);
    void addCapacityCheck(CapacityCheck check);

    // Sessions
    std::vector<Session> listSessions(const std::string& committeeId,
                                      const std::string& status) const;
    Session getSession(const std::string& id) const;
    Session createSession(const Json& body);
    Session updateSession(const std::string& id, const Json& body);
    void deleteSession(const std::string& id);
    Session activateSession(const std::string& id);
    Session deactivateSession(const std::string& id);
    Session completeSession(const std::string& id);

    // Dashboard numbers
    Json overview() const;
    Json liveSessions() const;  // public: what is in session now

    // DSA demonstrations (actual counts from the C searches)
    Json traceIdSearch(const std::string& type, const std::string& id) const;
    Json traceNameSearch(const std::string& query) const;
    Json listStructure() const;

private:
    std::vector<DependencyCheck> committeeChecks_;
    std::vector<CapacityCheck> capacityChecks_;

    std::string pickId(const Json& body, const char* prefix, int entity) const;
    void checkSessionRules(const Session& s, const std::string& ignoreId) const;
    Session saveSessionState(Session s);
};

// Registers every Module 1 endpoint, each with its permission check.
void registerModule1Routes(Router& router, ConferenceManager& manager, Auth& auth);

}  // namespace mun
