// MUNSphere — Module4_Attendance/Attendance.h
// One delegate's attendance in one session.
#pragma once

#include <string>

#include "Common.h"
#include "dsa_api.h"

namespace mun {

class AttendanceEntry : public Entity {
public:
    explicit AttendanceEntry(const AttendanceRec& rec);

    static std::string statusKey(int status);    // present|absent|late|checked_in
    static std::string statusLabel(int status);  // Present|Absent|Late|Checked in
    static int parseStatus(const std::string& key, bool allowCheckIn);  // throws 400

    int status() const { return rec_.status; }
    // Present and late both count as attending (and allow taking part).
    bool attended() const { return rec_.status == ATT_PRESENT || rec_.status == ATT_LATE; }

    Json toJson() const override;
    std::string entityName() const override { return "Attendance"; }

private:
    AttendanceRec rec_;
};

}  // namespace mun
