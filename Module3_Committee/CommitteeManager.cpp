// MUNSphere — Module3_Committee/CommitteeManager.cpp
#include "CommitteeManager.h"

#include <vector>

#include "Conference.h"
#include "DelegateManager.h"
#include "RoleManager.h"
#include "dsa_api.h"

namespace mun {

namespace {
std::vector<DelegateRec> allDelegates() {
    std::vector<DelegateRec> v(static_cast<std::size_t>(delegate_count()));
    v.resize(static_cast<std::size_t>(delegate_to_array(v.data(), static_cast<int>(v.size()))));
    return v;
}

std::vector<RosterEntry> rosterOf(const std::string& committeeId) {
    std::vector<RosterEntry> out;
    for (const DelegateRec& d : allDelegates()) {
        if (committeeId != d.committeeId) continue;
        RosterEntry e{};
        pers_copy_field(e.id, d.id, DSA_ID_LEN);
        pers_copy_field(e.name, d.name, DSA_NAME_LEN);
        pers_copy_field(e.portfolio, d.portfolio, DSA_NAME_LEN);
        out.push_back(e);
    }
    return out;
}
}  // namespace

CommitteeManager::CommitteeManager(DelegateManager& delegates, ConferenceManager& conferences)
    : delegates_(delegates), conferences_(conferences) {
    conferences.addCommitteeDependencyCheck([this](const std::string& id) -> std::string {
        int n = assignedCount(id);
        return n > 0 ? "This committee still has " + std::to_string(n) +
                           " delegate(s) assigned. Move them first."
                     : std::string();
    });
    conferences.addCapacityCheck([this](const std::string& id, int capacity) -> std::string {
        int n = assignedCount(id);
        return capacity < n ? "Capacity can't go below the " + std::to_string(n) +
                                  " delegates already assigned."
                            : std::string();
    });
}

int CommitteeManager::assignedCount(const std::string& committeeId) const {
    return static_cast<int>(rosterOf(committeeId).size());
}

Json CommitteeManager::roster(const std::string& committeeId, const std::string& sortKey) const {
    Committee c = conferences_.getCommittee(committeeId);
    int key = sortKey == "portfolio" ? ROSTER_BY_PORTFOLIO : sortKey == "id" ? ROSTER_BY_ID : ROSTER_BY_NAME;
    std::vector<RosterEntry> entries = rosterOf(committeeId);
    SortStats st{};
    roster_sort(entries.data(), static_cast<int>(entries.size()), key, &st);  // C insertion sort

    Json list = Json::array();
    for (const RosterEntry& e : entries) {
        Json j = Json::object();
        j.set("id", e.id).set("name", e.name).set("portfolio", e.portfolio);
        list.push(j);
    }
    Json sort = Json::object();
    sort.set("algorithm", "Insertion sort").set("key", key == ROSTER_BY_PORTFOLIO ? "portfolio" : key == ROSTER_BY_ID ? "id" : "name")
        .set("comparisons", static_cast<double>(st.comparisons))
        .set("moves", static_cast<double>(st.moves));
    Json out = Json::object();
    out.set("committee", c.toJson())
        .set("assigned", static_cast<double>(entries.size()))
        .set("capacity", c.capacity())
        .set("delegates", list)
        .set("sort", sort);
    return out;
}

Json CommitteeManager::summaries(const std::string& conferenceId) const {
    std::vector<DelegateRec> all = allDelegates();
    Json arr = Json::array();
    for (const Committee& c : conferences_.listCommittees(conferenceId, "")) {
        int assigned = 0, firstChoice = 0;
        for (const DelegateRec& d : all) {
            if (c.id() == d.committeeId) assigned++;
            if (d.status == REGISTRATION_PENDING && c.id() == d.prefs[0]) firstChoice++;
        }
        Json j = c.toJson();
        j.set("assigned", assigned)
            .set("ebCount", eb_count_by_committee(c.id().c_str()))
            .set("pendingFirstChoice", firstChoice);
        arr.push(j);
    }
    return arr;
}

Json CommitteeManager::assign(const std::string& delegateId, const Json& body) {
    Delegate d = delegates_.get(delegateId);
    std::string committeeId = requireString(body, "committeeId", DSA_ID_LEN - 1);
    std::string portfolio = requireString(body, "portfolio", 100);
    Committee c = conferences_.getCommittee(committeeId);
    if (c.conferenceId() != d.conferenceId())
        throw ApiError(409, "WRONG_CONFERENCE",
                       c.abbreviation() + " belongs to a different conference from this registration.");

    std::vector<RosterEntry> roster = rosterOf(committeeId);
    bool alreadyHere = d.committeeId() == committeeId;
    if (!alreadyHere && static_cast<int>(roster.size()) >= c.capacity())
        throw ApiError(409, "COMMITTEE_FULL", c.abbreviation() + " is full (" +
                                                  std::to_string(c.capacity()) + " delegates).");
    int clash = roster_find_portfolio(roster.data(), static_cast<int>(roster.size()),
                                      portfolio.c_str(), delegateId.c_str());
    if (clash >= 0)
        throw ApiError(409, "PORTFOLIO_TAKEN", portfolio + " is already held by " +
                                                   roster[static_cast<std::size_t>(clash)].name + " in " +
                                                   c.abbreviation() + ".");
    d.assign(committeeId, portfolio);
    delegates_.save(d, false);
    return d.toJson();
}

Json CommitteeManager::unassign(const std::string& delegateId) {
    Delegate d = delegates_.get(delegateId);
    if (!d.assigned()) throw ApiError(409, "INVALID_STATE", "This delegate isn't assigned to a committee.");
    d.unassign();
    delegates_.save(d, false);
    return d.toJson();
}

void registerCommitteeRoutes(Router& r, CommitteeManager& cm, Auth& auth) {
    r.get("/api/committees/:id/roster", [&cm, &auth](const HttpRequest& q) {
        // Admin, or an EB member of THIS committee.
        RoleManager::requireCommittee(auth.require(q), q.param("id"), perm::VIEW);
        return HttpResponse::ok(cm.roster(q.param("id"), q.queryParam("sort")));
    });
    r.get("/api/committee-summaries", [&cm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(cm.summaries(q.queryParam("conferenceId")));
    });
    r.post("/api/delegates/:id/assign", [&cm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(cm.assign(q.param("id"), q.json()));
    });
    r.post("/api/delegates/:id/unassign", [&cm, &auth](const HttpRequest& q) {
        RoleManager::requireAdmin(auth.require(q));
        return HttpResponse::ok(cm.unassign(q.param("id")));
    });
}

}  // namespace mun
