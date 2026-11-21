// MUNSphere — Module5_SpeakerList/Speaker.cpp
#include "Speaker.h"

#include "DelegateManager.h"

namespace mun {

PointOfInformation::PointOfInformation(const PoiRec& rec) : Entity(rec.id), rec_(rec) {}

PointOfInformation PointOfInformation::create(const std::string& id, const std::string& sessionId,
                                              const std::string& committeeId, const std::string& delegateId,
                                              const std::string& targetId, const std::string& text) {
    std::string t = validate::trim(text);
    if (t.empty()) throw ApiError(400, "VALIDATION", "Write the question you want to ask.");
    if (t.size() > DSA_TEXT_LEN - 1) throw ApiError(400, "VALIDATION", "Keep the POI under 300 characters.");
    if (delegateId == targetId) throw ApiError(400, "VALIDATION", "You can't raise a POI to yourself.");
    PoiRec r{};
    pers_copy_field(r.id, id.c_str(), DSA_ID_LEN);
    pers_copy_field(r.sessionId, sessionId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.committeeId, committeeId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.delegateId, delegateId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.targetId, targetId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.text, t.c_str(), DSA_TEXT_LEN);
    pers_copy_field(r.createdAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
    r.status = POI_PENDING;
    return PointOfInformation(r);
}

std::string PointOfInformation::statusKey(int s) {
    switch (s) {
    case POI_PENDING: return "pending";
    case POI_ACCEPTED: return "accepted";
    case POI_REJECTED: return "rejected";
    case POI_WITHDRAWN: return "withdrawn";
    default: return "unknown";
    }
}

void PointOfInformation::requirePending() const {
    if (rec_.status != POI_PENDING)
        throw ApiError(409, "INVALID_STATE", "This POI was already " + statusKey(rec_.status) + ".");
}

void PointOfInformation::accept(const std::string& by) {
    requirePending();
    rec_.status = POI_ACCEPTED;
    pers_copy_field(rec_.decidedBy, by.c_str(), DSA_ID_LEN);
}

void PointOfInformation::reject(const std::string& by, const std::string& reason) {
    requirePending();
    std::string r = validate::trim(reason);
    if (r.empty()) throw ApiError(400, "VALIDATION", "Give the delegate a reason for the rejection.");
    if (r.size() > DSA_TEXT_LEN - 1) throw ApiError(400, "VALIDATION", "Reason is too long (max 300 characters).");
    rec_.status = POI_REJECTED;
    pers_copy_field(rec_.decidedBy, by.c_str(), DSA_ID_LEN);
    pers_copy_field(rec_.reason, r.c_str(), DSA_TEXT_LEN);
}

void PointOfInformation::withdraw(const std::string& by) {
    // Ownership first: a stranger mustn't learn anything about the POI's state.
    if (by != rec_.delegateId) throw ApiError(403, "FORBIDDEN", "Only the delegate who raised a POI can withdraw it.");
    requirePending();
    rec_.status = POI_WITHDRAWN;
}

Json PointOfInformation::toJson() const {
    Json j = Json::object();
    j.set("id", rec_.id)
        .set("sessionId", rec_.sessionId)
        .set("committeeId", rec_.committeeId)
        .set("from", delegateBrief(rec_.delegateId))
        .set("to", delegateBrief(rec_.targetId))
        .set("text", rec_.text)
        .set("createdAt", rec_.createdAt)
        .set("status", statusKey(rec_.status))
        .set("decidedBy", rec_.decidedBy)
        .set("reason", rec_.reason);
    return j;
}

}  // namespace mun
