// MUNSphere — Module8_Scoring/Score.cpp
#include "Score.h"

#include <cmath>

#include "DelegateManager.h"

namespace mun {

Marksheet::Marksheet(const MarksheetRec& rec) : Entity(rec.id), rec_(rec) {}

Marksheet Marksheet::create(const std::string& id, const std::string& delegateId, const std::string& committeeId,
                            const std::string& by, const std::vector<CriterionRec>& criteria,
                            double attendancePercent) {
    if (criteria.empty()) throw ApiError(409, "NO_CRITERIA", "The Admin hasn't set any scoring criteria.");
    MarksheetRec r{};
    pers_copy_field(r.id, id.c_str(), DSA_ID_LEN);
    pers_copy_field(r.delegateId, delegateId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.committeeId, committeeId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.createdBy, by.c_str(), DSA_ID_LEN);
    r.criteriaCount = static_cast<int>(criteria.size());
    for (std::size_t i = 0; i < criteria.size(); ++i) {
        pers_copy_field(r.critKey[i], criteria[i].key, DSA_ID_LEN);
        pers_copy_field(r.critName[i], criteria[i].name, CRIT_NAME_LEN);
        r.critMax[i] = criteria[i].maxMarks;
        // The attendance criterion starts from the delegate's real attendance (Module 4).
        r.marks[i] = criteria[i].fromAttendance
                         ? static_cast<int>(std::lround(attendancePercent / 100.0 * criteria[i].maxMarks))
                         : 0;
    }
    r.status = SHEET_DRAFT;
    r.revision = 1;
    pers_copy_field(r.updatedAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
    sheet_compute_total(&r);
    return Marksheet(r);
}

std::string Marksheet::statusKey(int s) {
    static const char* k[SHEET_STATUS_COUNT] = {"draft", "submitted", "returned", "approved", "published"};
    return s >= 0 && s < SHEET_STATUS_COUNT ? k[s] : "unknown";
}

std::string Marksheet::actionKey(int a) {
    static const char* k[SHEET_ACTION_COUNT] = {"created", "saved", "submitted", "returned", "approved", "published"};
    return a >= 0 && a < SHEET_ACTION_COUNT ? k[a] : "unknown";
}

double Marksheet::percent() const {
    return rec_.maxTotal ? std::round(rec_.total * 1000.0 / rec_.maxTotal) / 10.0 : 0.0;
}

int Marksheet::markFor(const std::string& key) const {
    for (int i = 0; i < rec_.criteriaCount; ++i)
        if (key == rec_.critKey[i]) return rec_.marks[i];
    return -1;
}

int Marksheet::maxFor(const std::string& key) const {
    for (int i = 0; i < rec_.criteriaCount; ++i)
        if (key == rec_.critKey[i]) return rec_.critMax[i];
    return 0;
}

void Marksheet::setMarks(const Json& marks) {
    if (!editable()) throw ApiError(409, "INVALID_STATE", "This marksheet is " + statusKey(rec_.status) + " and can't be edited.");
    if (!marks.isObject()) throw ApiError(400, "VALIDATION", "Marks must be given per criterion.");
    MarksheetRec next = rec_;
    for (const auto& kv : marks.asObject()) {
        int idx = -1;
        for (int i = 0; i < next.criteriaCount; ++i)
            if (kv.first == next.critKey[i]) idx = i;
        if (idx < 0) throw ApiError(400, "VALIDATION", "'" + kv.first + "' isn't a criterion on this marksheet.");
        const Json& v = kv.second;
        if (!v.isNumber() || std::floor(v.asNumber()) != v.asNumber() || v.asNumber() < 0 ||
            v.asNumber() > next.critMax[idx])
            throw ApiError(400, "VALIDATION", std::string(next.critName[idx]) + " must be a whole number from 0 to " +
                                                  std::to_string(next.critMax[idx]) + ".");
        next.marks[idx] = static_cast<int>(v.asNumber());
    }
    if (sheet_compute_total(&next) != DSA_OK) throw ApiError(400, "VALIDATION", "A mark is out of range.");
    rec_ = next;
}

void Marksheet::setRemarks(const std::string& text) {
    std::string t = validate::trim(text);
    if (t.size() > DSA_TEXT_LEN - 1) throw ApiError(400, "VALIDATION", "Remarks are too long (max 300 characters).");
    if (!editable()) throw ApiError(409, "INVALID_STATE", "This marksheet can't be edited now.");
    pers_copy_field(rec_.remarks, t.c_str(), DSA_TEXT_LEN);
}

void Marksheet::submit() {
    if (!editable()) throw ApiError(409, "INVALID_STATE", "Only a draft or returned marksheet can be submitted.");
    rec_.status = SHEET_SUBMITTED;
}

void Marksheet::returnForCorrection(const std::string& note) {
    if (rec_.status != SHEET_SUBMITTED && rec_.status != SHEET_APPROVED)
        throw ApiError(409, "INVALID_STATE", "Only a submitted or approved marksheet can be returned.");
    std::string n = validate::trim(note);
    if (n.empty()) throw ApiError(400, "VALIDATION", "Tell the Executive Board what to correct.");
    if (n.size() > DSA_TEXT_LEN - 1) throw ApiError(400, "VALIDATION", "Note is too long (max 300 characters).");
    rec_.status = SHEET_RETURNED;
    pers_copy_field(rec_.adminNote, n.c_str(), DSA_TEXT_LEN);
}

void Marksheet::approve() {
    if (rec_.status != SHEET_SUBMITTED) throw ApiError(409, "INVALID_STATE", "Only a submitted marksheet can be approved.");
    rec_.status = SHEET_APPROVED;
}

void Marksheet::publish() {
    if (rec_.status != SHEET_APPROVED) throw ApiError(409, "INVALID_STATE", "Only an approved marksheet can be published.");
    rec_.status = SHEET_PUBLISHED;
}

void Marksheet::stamp(const std::string& by) {
    rec_.revision++;
    pers_copy_field(rec_.updatedAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
    (void)by;
}

SheetRevisionRec Marksheet::revision(int action, const std::string& by, const std::string& note) const {
    SheetRevisionRec v{};
    pers_copy_field(v.marksheetId, rec_.id, DSA_ID_LEN);
    v.revision = rec_.revision;
    v.action = action;
    pers_copy_field(v.by, by.c_str(), DSA_ID_LEN);
    pers_copy_field(v.at, nowTimestamp().c_str(), DSA_STAMP_LEN);
    v.total = rec_.total;
    v.marksCount = rec_.criteriaCount;
    for (int i = 0; i < rec_.criteriaCount; ++i) v.marks[i] = rec_.marks[i];
    pers_copy_field(v.note, note.c_str(), DSA_TEXT_LEN);
    return v;
}

Json Marksheet::toJson(bool forDelegate) const {
    Json crit = Json::array();
    for (int i = 0; i < rec_.criteriaCount; ++i) {
        Json c = Json::object();
        c.set("key", rec_.critKey[i]).set("name", rec_.critName[i]).set("max", rec_.critMax[i]).set("mark", rec_.marks[i]);
        crit.push(c);
    }
    Json j = Json::object();
    j.set("id", rec_.id)
        .set("delegate", delegateBrief(rec_.delegateId))
        .set("committeeId", rec_.committeeId)
        .set("criteria", crit)
        .set("total", rec_.total)
        .set("maxTotal", rec_.maxTotal)
        .set("percent", percent())
        .set("status", statusKey(rec_.status))
        .set("remarks", rec_.remarks)
        .set("updatedAt", rec_.updatedAt);
    if (!forDelegate)  // internal workflow details stay with the EB and Admin
        j.set("revision", rec_.revision).set("adminNote", rec_.adminNote).set("createdBy", rec_.createdBy);
    return j;
}

}  // namespace mun
