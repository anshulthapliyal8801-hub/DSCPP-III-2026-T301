// MUNSphere — Module5_SpeakerList/Speaker.h
// Point of Information: raised by a delegate to the current speaker,
// decided by the Executive Board.
//
//   pending --accept--> accepted
//      |   --reject (reason required)--> rejected
//      +--- withdraw (by the delegate) --> withdrawn
#pragma once

#include <string>

#include "Common.h"
#include "dsa_api.h"

namespace mun {

class PointOfInformation : public Entity {
public:
    explicit PointOfInformation(const PoiRec& rec);
    static PointOfInformation create(const std::string& id, const std::string& sessionId,
                                     const std::string& committeeId, const std::string& delegateId,
                                     const std::string& targetId, const std::string& text);

    static std::string statusKey(int s);
    int status() const { return rec_.status; }
    const std::string delegateId() const { return rec_.delegateId; }
    const std::string sessionId() const { return rec_.sessionId; }

    void accept(const std::string& by);
    void reject(const std::string& by, const std::string& reason);
    void withdraw(const std::string& by);

    PoiRec toRecord() const { return rec_; }
    Json toJson() const override;
    std::string entityName() const override { return "POI"; }

private:
    PoiRec rec_;
    void requirePending() const;
};

}  // namespace mun
