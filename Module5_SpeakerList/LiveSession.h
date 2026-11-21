// MUNSphere — Module5_SpeakerList/LiveSession.h
// One endpoint that assembles a session's live state from Modules 4-7
// (attendance, speakers and POIs, motions, voting), shaped for the
// viewer's role. Lives with the speaker list because the speaker list is
// the centre of a live session. Added file: it composes several modules,
// so it belongs to none of their managers.
#pragma once

#include "API.h"
#include "AttendanceManager.h"
#include "Auth.h"
#include "Conference.h"
#include "MotionManager.h"
#include "SpeakerManager.h"
#include "VotingManager.h"

namespace mun {

void registerLiveRoutes(Router& router, Auth& auth, ConferenceManager& conf, AttendanceManager& att,
                        SpeakerManager& speakers, MotionManager& motions, VotingManager& votes);

}  // namespace mun
