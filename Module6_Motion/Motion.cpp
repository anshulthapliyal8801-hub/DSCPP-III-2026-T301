// MUNSphere — Module6_Motion/Motion.cpp
#include "Motion.h"

#include "DelegateManager.h"

namespace mun {

namespace {
// Order of precedence: the most disruptive motion is considered first.
const MotionTypeInfo kTypes[MOTION_TYPE_COUNT] = {
    {MOT_ADJOURN, "adjourn", "Adjourn the meeting", false, false, false, MAJ_SIMPLE},
    {MOT_SUSPEND, "suspend", "Suspend the meeting", false, false, false, MAJ_SIMPLE},
    {MOT_CLOSE_DEBATE, "close_debate", "Close debate", false, false, false, MAJ_TWO_THIRDS},
    {MOT_UNMODERATED, "unmoderated", "Unmoderated caucus", true, false, false, MAJ_SIMPLE},
    {MOT_MODERATED, "moderated", "Moderated caucus", true, true, true, MAJ_SIMPLE},
    {MOT_INTRODUCE_DRAFT, "introduce_draft", "Introduce a draft resolution", false, false, true, MAJ_SIMPLE},
    {MOT_SPEAKING_TIME, "speaking_time", "Set the speaking time", false, true, false, MAJ_SIMPLE},
};
}  // namespace

Motion::Motion(const MotionRec& rec) : Entity(rec.id), rec_(rec) {}

const MotionTypeInfo& Motion::info(int type) {
    if (type < 0 || type >= MOTION_TYPE_COUNT) throw ApiError(500, "BAD_RECORD", "Unknown motion type.");
    return kTypes[type];
}

int Motion::parseType(const std::string& key) {
    for (const MotionTypeInfo& t : kTypes)
        if (key == t.key) return t.type;
    throw ApiError(400, "VALIDATION", "Unknown motion type '" + key + "'.");
}

Json Motion::typeCatalog() {
    Json arr = Json::array();
    for (const MotionTypeInfo& t : kTypes) {
        Json j = Json::object();
        j.set("key", t.key).set("label", t.label).set("precedence", t.type + 1)
            .set("needsMinutes", t.needsMinutes).set("needsSpeakingTime", t.needsSpeakingTime)
            .set("needsTopic", t.needsTopic).set("majority", t.majority == MAJ_TWO_THIRDS ? "two_thirds" : "simple");
        arr.push(j);
    }
    return arr;
}

std::string Motion::statusKey(int s) {
    static const char* k[MOTION_STATUS_COUNT] = {"pending", "voting", "passed", "failed", "withdrawn", "out_of_order"};
    return s >= 0 && s < MOTION_STATUS_COUNT ? k[s] : "unknown";
}

Motion Motion::create(const std::string& id, const std::string& sessionId, const std::string& committeeId,
                      const std::string& proposerId, const std::string& typeKey, const Json& body) {
    int type = parseType(typeKey);
    const MotionTypeInfo& t = kTypes[type];
    MotionRec r{};
    pers_copy_field(r.id, id.c_str(), DSA_ID_LEN);
    pers_copy_field(r.sessionId, sessionId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.committeeId, committeeId.c_str(), DSA_ID_LEN);
    pers_copy_field(r.proposerId, proposerId.c_str(), DSA_ID_LEN);
    r.type = type;
    if (t.needsTopic) {
        std::string topic = requireString(body, "topic", DSA_TEXT_LEN - 1);
        pers_copy_field(r.topic, topic.c_str(), DSA_TEXT_LEN);
    } else {
        pers_copy_field(r.topic, optionalString(body, "topic", DSA_TEXT_LEN - 1, "").c_str(), DSA_TEXT_LEN);
    }
    if (t.needsMinutes) r.totalMinutes = requireInt(body, "totalMinutes", 1, 60);
    if (t.needsSpeakingTime) r.speakingSeconds = requireInt(body, "speakingSeconds", 10, 300);
    r.status = MS_PENDING;
    pers_copy_field(r.createdAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
    r.seq = motion_next_seq();
    return Motion(r);
}

void Motion::setVote(const std::string& voteId) { pers_copy_field(rec_.voteId, voteId.c_str(), DSA_ID_LEN); }

std::string Motion::describe() const {
    const MotionTypeInfo& t = info(rec_.type);
    std::string s = t.label;
    if (t.needsMinutes) s += ", " + std::to_string(rec_.totalMinutes) + " min";
    if (t.needsSpeakingTime) s += ", " + std::to_string(rec_.speakingSeconds) + " s" + (t.needsMinutes ? " each" : "");
    if (rec_.topic[0]) s += ": " + std::string(rec_.topic);
    return s;
}

Json Motion::toJson() const {
    const MotionTypeInfo& t = info(rec_.type);
    Json j = Json::object();
    j.set("id", rec_.id)
        .set("sessionId", rec_.sessionId)
        .set("type", t.key)
        .set("typeLabel", t.label)
        .set("precedence", rec_.type + 1)
        .set("description", describe())
        .set("topic", rec_.topic)
        .set("totalMinutes", rec_.totalMinutes)
        .set("speakingSeconds", rec_.speakingSeconds)
        .set("status", statusKey(rec_.status))
        .set("proposer", delegateBrief(rec_.proposerId))
        .set("createdAt", rec_.createdAt)
        .set("voteId", rec_.voteId)
        .set("majority", t.majority == MAJ_TWO_THIRDS ? "two_thirds" : "simple");
    return j;
}

}  // namespace mun
