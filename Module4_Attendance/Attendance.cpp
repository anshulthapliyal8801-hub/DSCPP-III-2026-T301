// MUNSphere — Module4_Attendance/Attendance.cpp
#include "Attendance.h"

namespace mun {

AttendanceEntry::AttendanceEntry(const AttendanceRec& rec)
    : Entity(std::string(rec.sessionId) + ":" + rec.delegateId), rec_(rec) {}

std::string AttendanceEntry::statusKey(int s) {
    switch (s) {
    case ATT_PRESENT: return "present";
    case ATT_ABSENT: return "absent";
    case ATT_LATE: return "late";
    case ATT_CHECKED_IN: return "checked_in";
    default: return "unknown";
    }
}

std::string AttendanceEntry::statusLabel(int s) {
    switch (s) {
    case ATT_PRESENT: return "Present";
    case ATT_ABSENT: return "Absent";
    case ATT_LATE: return "Late";
    case ATT_CHECKED_IN: return "Checked in";
    default: return "Unknown";
    }
}

int AttendanceEntry::parseStatus(const std::string& key, bool allowCheckIn) {
    for (int s = 0; s < ATT_STATUS_COUNT; ++s)
        if (key == statusKey(s) && (allowCheckIn || s != ATT_CHECKED_IN)) return s;
    throw ApiError(400, "VALIDATION", "Status must be present, late or absent.");
}

Json AttendanceEntry::toJson() const {
    Json j = Json::object();
    j.set("sessionId", rec_.sessionId)
        .set("delegateId", rec_.delegateId)
        .set("status", statusKey(rec_.status))
        .set("statusLabel", statusLabel(rec_.status))
        .set("markedBy", rec_.markedBy)
        .set("markedAt", rec_.markedAt);
    return j;
}

}  // namespace mun
