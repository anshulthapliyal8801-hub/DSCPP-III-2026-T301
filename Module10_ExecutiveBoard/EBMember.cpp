// MUNSphere — Module10_ExecutiveBoard/EBMember.cpp
#include "EBMember.h"

#include "RoleManager.h"

namespace mun {

EBMember::EBMember(User account, const EbRec& rec)
    : Person(std::move(account), rec.email[0] ? rec.email : "unknown@example.com"),
      committeeId_(rec.committeeId), ebRole_(rec.ebRole), permissions_(rec.permissions) {}

std::unique_ptr<EBMember> EBMember::create(User account, const EbRec& rec) {
    switch (rec.ebRole) {
    case EB_CHAIRPERSON: return std::make_unique<Chairperson>(std::move(account), rec);
    case EB_MODERATOR: return std::make_unique<Moderator>(std::move(account), rec);
    case EB_SCRIBE: return std::make_unique<Scribe>(std::move(account), rec);
    default: throw ApiError(500, "BAD_RECORD", "Stored EB role is invalid for " + account.id() + ".");
    }
}

int EBMember::defaultPermissionsFor(int ebRole) {
    EbRec probe{};
    probe.ebRole = ebRole;
    pers_copy_field(probe.email, "probe@example.com", DSA_NAME_LEN);
    // Asks the subclass itself: the rule lives in one place, the override.
    return create(User("PROBE", ROLE_EB, "Probe"), probe)->defaultPermissions();
}

void EBMember::setCommittee(const std::string& committeeId) {
    if (committeeId.empty()) throw ApiError(400, "VALIDATION", "An EB member needs a committee.");
    committeeId_ = committeeId;
}

void EBMember::setPermissions(int mask) { permissions_ = (mask & perm::ALL) | perm::VIEW; }

EbRec EBMember::toRecord() const {
    EbRec r{};
    pers_copy_field(r.id, id_.c_str(), DSA_ID_LEN);
    pers_copy_field(r.committeeId, committeeId_.c_str(), DSA_ID_LEN);
    r.ebRole = ebRole_;
    r.permissions = permissions_;
    pers_copy_field(r.email, email_.c_str(), DSA_NAME_LEN);
    return r;
}

std::string EBMember::summary() const {
    CommitteeRec c;
    std::string where = committee_find(committeeId_.c_str(), &c, nullptr) == DSA_OK
                            ? std::string(c.abbreviation) : committeeId_;
    return roleTitle() + ", " + where;
}

Json EBMember::toJson() const {
    Json j = Person::toJson();  // reuse the base part, then extend
    j.set("committeeId", committeeId_)
        .set("ebRole", roleTitle())
        .set("permissions", RoleManager::permissionsToJson(permissions_))
        .set("defaultPermissions", RoleManager::permissionsToJson(defaultPermissions()))
        .set("responsibilities", responsibilities());
    return j;
}

int Chairperson::defaultPermissions() const { return perm::ALL; }
std::string Chairperson::responsibilities() const {
    return "Presides over the committee, rules on motions, evaluates delegates and submits marksheets.";
}

int Moderator::defaultPermissions() const {
    return perm::VIEW | perm::RUN_SESSIONS | perm::SPEAKERS | perm::POI | perm::MOTIONS |
           perm::VOTING | perm::ATTENDANCE;
}
std::string Moderator::responsibilities() const {
    return "Runs the floor: speaker list, timer, POIs, motions and voting.";
}

int Scribe::defaultPermissions() const {
    return perm::VIEW | perm::ATTENDANCE | perm::REPORTS;
}
std::string Scribe::responsibilities() const {
    return "Keeps the record: attendance and committee reports.";
}

}  // namespace mun
