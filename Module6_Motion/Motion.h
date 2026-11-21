// MUNSphere — Module6_Motion/Motion.h
// A motion and the rules of each motion type.
#pragma once

#include <string>

#include "Common.h"
#include "dsa_api.h"

namespace mun {

struct MotionTypeInfo {
    int type;              // MotionType; also the precedence (lower = first)
    const char* key;
    const char* label;
    bool needsMinutes;     // caucuses have a total duration
    bool needsSpeakingTime;
    bool needsTopic;
    int majority;          // MajorityRule when put to a vote
};

class Motion : public Entity {
public:
    explicit Motion(const MotionRec& rec);
    // Validates the fields the type requires (throws 400).
    static Motion create(const std::string& id, const std::string& sessionId,
                         const std::string& committeeId, const std::string& proposerId,
                         const std::string& typeKey, const Json& body);

    static const MotionTypeInfo& info(int type);
    static int parseType(const std::string& key);
    static Json typeCatalog();
    static std::string statusKey(int s);

    int type() const { return rec_.type; }
    int status() const { return rec_.status; }
    const MotionRec& record() const { return rec_; }
    std::string describe() const;  // "Moderated caucus, 15 min, 60 s each: Climate finance"

    void setStatus(int s) { rec_.status = s; }
    void setVote(const std::string& voteId);

    Json toJson() const override;
    std::string entityName() const override { return "Motion"; }

private:
    MotionRec rec_;
};

}  // namespace mun
