// MUNSphere — Module8_Scoring/Score.h
// A delegate's marksheet and its approval workflow:
//
//   draft --submit (EB)--> submitted --approve (Admin)--> approved --publish (Admin)--> published
//     ^                       |                              |
//     +---- edit (EB) ---- returned <-- return with a note (Admin)
//
// Marks are checked against each criterion's maximum. The criteria are a
// snapshot taken when the marksheet was created.
#pragma once

#include <string>
#include <vector>

#include "Common.h"
#include "dsa_api.h"

namespace mun {

class Marksheet : public Entity {
public:
    explicit Marksheet(const MarksheetRec& rec);
    static Marksheet create(const std::string& id, const std::string& delegateId,
                            const std::string& committeeId, const std::string& by,
                            const std::vector<CriterionRec>& criteria, double attendancePercent);

    static std::string statusKey(int s);
    static std::string actionKey(int a);

    int status() const { return rec_.status; }
    const MarksheetRec& record() const { return rec_; }
    double percent() const;
    int markFor(const std::string& key) const;  // -1 if not on this sheet
    int maxFor(const std::string& key) const;
    bool editable() const { return rec_.status == SHEET_DRAFT || rec_.status == SHEET_RETURNED; }

    void setMarks(const Json& marks);           // {"research": 18, ...}; throws 400
    void setRemarks(const std::string& text);
    void submit();
    void returnForCorrection(const std::string& note);
    void approve();
    void publish();
    void stamp(const std::string& by);          // revision++, updatedAt

    // Revision record for the history list.
    SheetRevisionRec revision(int action, const std::string& by, const std::string& note) const;

    Json toJson(bool forDelegate) const;
    Json toJson() const override { return toJson(false); }
    std::string entityName() const override { return "Marksheet"; }

private:
    MarksheetRec rec_;
};

}  // namespace mun
