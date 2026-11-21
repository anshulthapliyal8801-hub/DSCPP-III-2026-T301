// MUNSphere — Module2_Delegate/Delegate.cpp
#include "Delegate.h"

namespace mun {

Delegate::Delegate(User account, const DelegateRec& rec)
    : Person(std::move(account), rec.email[0] ? rec.email : "unknown@example.com"), rec_(rec) {}

std::string Delegate::statusKey(int s) {
    switch (s) {
    case REGISTRATION_NONE: return "none";
    case REGISTRATION_PENDING: return "pending";
    case REGISTRATION_APPROVED: return "approved";
    case REGISTRATION_REJECTED: return "rejected";
    default: return "unknown";
    }
}

std::vector<std::string> Delegate::preferences() const {
    std::vector<std::string> out;
    for (int k = 0; k < DELEGATE_PREFS; ++k)
        if (rec_.prefs[k][0]) out.emplace_back(rec_.prefs[k]);
    return out;
}

void Delegate::setName(const std::string& name) {
    account_.setName(name);  // validates
    pers_copy_field(rec_.name, account_.name().c_str(), DSA_NAME_LEN);
}

void Delegate::setInstitution(const std::string& v) {
    std::string s = validate::trim(v);
    if (s.size() > DSA_NAME_LEN - 1)
        throw ApiError(400, "VALIDATION", "Institution is too long (max 100 characters).");
    pers_copy_field(rec_.institution, s.c_str(), DSA_NAME_LEN);
}

void Delegate::apply(const std::string& conferenceId, const std::vector<std::string>& prefs) {
    if (rec_.status == REGISTRATION_APPROVED)
        throw ApiError(409, "INVALID_STATE",
                       "Your registration is already approved. Contact the admin to change it.");
    if (prefs.empty() || prefs.size() > DELEGATE_PREFS)
        throw ApiError(400, "VALIDATION", "Choose between 1 and 3 committee preferences.");
    pers_copy_field(rec_.conferenceId, conferenceId.c_str(), DSA_ID_LEN);
    for (int k = 0; k < DELEGATE_PREFS; ++k)
        pers_copy_field(rec_.prefs[k], k < static_cast<int>(prefs.size()) ? prefs[static_cast<std::size_t>(k)].c_str() : "",
                        DSA_ID_LEN);
    rec_.status = REGISTRATION_PENDING;
    rec_.note[0] = '\0';
    pers_copy_field(rec_.registeredAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
}

void Delegate::withdraw() {
    if (rec_.status == REGISTRATION_APPROVED)
        throw ApiError(409, "INVALID_STATE", "An approved registration can only be changed by the admin.");
    if (rec_.status == REGISTRATION_NONE)
        throw ApiError(409, "INVALID_STATE", "You haven't registered for a conference.");
    rec_.status = REGISTRATION_NONE;
    rec_.conferenceId[0] = '\0';
    for (int k = 0; k < DELEGATE_PREFS; ++k) rec_.prefs[k][0] = '\0';
    rec_.note[0] = '\0';
    rec_.registeredAt[0] = '\0';
}

void Delegate::approve() {
    if (rec_.status != REGISTRATION_PENDING)
        throw ApiError(409, "INVALID_STATE", "Only a pending registration can be approved.");
    rec_.status = REGISTRATION_APPROVED;
    rec_.note[0] = '\0';
}

void Delegate::reject(const std::string& reason) {
    if (rec_.status != REGISTRATION_PENDING)
        throw ApiError(409, "INVALID_STATE", "Only a pending registration can be rejected.");
    std::string r = validate::trim(reason);
    if (r.empty()) throw ApiError(400, "VALIDATION", "Give the delegate a reason for the rejection.");
    if (r.size() > DSA_TEXT_LEN - 1) throw ApiError(400, "VALIDATION", "Reason is too long (max 300 characters).");
    rec_.status = REGISTRATION_REJECTED;
    pers_copy_field(rec_.note, r.c_str(), DSA_TEXT_LEN);
}

void Delegate::assign(const std::string& committeeId, const std::string& portfolio) {
    if (rec_.status != REGISTRATION_APPROVED)
        throw ApiError(409, "INVALID_STATE", "Approve the registration before assigning a committee.");
    std::string p = validate::trim(portfolio);
    if (p.empty()) throw ApiError(400, "VALIDATION", "Enter the country or portfolio.");
    if (p.size() > DSA_NAME_LEN - 1) throw ApiError(400, "VALIDATION", "Portfolio is too long (max 100 characters).");
    pers_copy_field(rec_.committeeId, committeeId.c_str(), DSA_ID_LEN);
    pers_copy_field(rec_.portfolio, p.c_str(), DSA_NAME_LEN);
}

void Delegate::unassign() {
    rec_.committeeId[0] = '\0';
    rec_.portfolio[0] = '\0';
}

DelegateRec Delegate::toRecord() const {
    DelegateRec r = rec_;
    pers_copy_field(r.name, name().c_str(), DSA_NAME_LEN);
    pers_copy_field(r.email, email_.c_str(), DSA_NAME_LEN);
    return r;
}

std::string Delegate::summary() const {
    if (assigned()) {
        CommitteeRec c;
        std::string where = committee_find(rec_.committeeId, &c, nullptr) == DSA_OK
                                ? std::string(c.abbreviation) : std::string(rec_.committeeId);
        return std::string(rec_.portfolio) + " in " + where;
    }
    switch (rec_.status) {
    case REGISTRATION_PENDING: return "Registration pending";
    case REGISTRATION_APPROVED: return "Approved, awaiting committee";
    case REGISTRATION_REJECTED: return "Registration rejected";
    default: return "Not registered";
    }
}

Json Delegate::toJson() const {
    Json j = Person::toJson();
    Json prefs = Json::array();
    for (const std::string& p : preferences()) prefs.push(p);
    j.set("institution", rec_.institution)
        .set("status", statusKey(rec_.status))
        .set("conferenceId", rec_.conferenceId)
        .set("preferences", prefs)
        .set("note", rec_.note)
        .set("committeeId", rec_.committeeId)
        .set("portfolio", rec_.portfolio)
        .set("registeredAt", rec_.registeredAt)
        .set("createdAt", account_.createdAt());
    return j;
}

}  // namespace mun
