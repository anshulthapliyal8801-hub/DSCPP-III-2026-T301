// MUNSphere — Module3_Committee/Committee.h
// The Committee entity. Defined once here and used by Module 1 (creation,
// sessions) and Module 3 (capacity, delegate and EB assignment).
#pragma once

#include <string>

#include "Common.h"
#include "dsa_api.h"

namespace mun {

class Committee : public Entity {
public:
    static constexpr int kMinCapacity = 1;
    static constexpr int kMaxCapacity = 500;

    Committee(std::string id, std::string conferenceId, std::string name,
              std::string abbreviation, std::string agenda, int capacity);

    // Conversion to and from the C record stored in the linked list.
    static Committee fromRecord(const CommitteeRec& r);
    CommitteeRec toRecord() const;

    const std::string& conferenceId() const { return conferenceId_; }
    const std::string& name() const { return name_; }
    const std::string& abbreviation() const { return abbreviation_; }
    const std::string& agenda() const { return agenda_; }
    int capacity() const { return capacity_; }

    // Setters validate, so an invalid Committee can never exist
    // (encapsulation: fields are private and change only through rules).
    void setName(const std::string& v);
    void setAbbreviation(const std::string& v);
    void setAgenda(const std::string& v);
    void setCapacity(int v);

    Json toJson() const override;
    std::string entityName() const override { return "Committee"; }

private:
    std::string conferenceId_;
    std::string name_;
    std::string abbreviation_;
    std::string agenda_;
    int capacity_ = kMinCapacity;
};

}  // namespace mun
