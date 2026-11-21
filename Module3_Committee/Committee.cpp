// MUNSphere — Module3_Committee/Committee.cpp
#include "Committee.h"

#include <cstring>

namespace mun {

static void copyInto(char* dst, const std::string& src, std::size_t cap) {
    std::size_t n = src.size() < cap - 1 ? src.size() : cap - 1;
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

Committee::Committee(std::string id, std::string conferenceId, std::string name,
                     std::string abbreviation, std::string agenda, int capacity)
    : Entity(std::move(id)), conferenceId_(std::move(conferenceId)) {
    setName(name);
    setAbbreviation(abbreviation);
    setAgenda(agenda);
    setCapacity(capacity);
}

Committee Committee::fromRecord(const CommitteeRec& r) {
    // Records in the store were validated on the way in. Reading must
    // never throw (a hand-edited file could break a rule), so start from
    // a valid object and then copy the stored fields in exactly.
    Committee c(r.id, r.conferenceId, "x", "x", "", kMinCapacity);
    c.name_ = r.name;
    c.abbreviation_ = r.abbreviation;
    c.agenda_ = r.agenda;
    c.capacity_ = r.capacity;
    return c;
}

CommitteeRec Committee::toRecord() const {
    CommitteeRec r{};
    copyInto(r.id, id_, DSA_ID_LEN);
    copyInto(r.conferenceId, conferenceId_, DSA_ID_LEN);
    copyInto(r.name, name_, DSA_NAME_LEN);
    copyInto(r.abbreviation, abbreviation_, DSA_SHORT_LEN);
    copyInto(r.agenda, agenda_, DSA_TEXT_LEN);
    r.capacity = capacity_;
    return r;
}

void Committee::setName(const std::string& v) {
    std::string s = validate::trim(v);
    if (s.empty()) throw ApiError(400, "VALIDATION", "Committee name can't be empty.");
    if (s.size() > DSA_NAME_LEN - 1)
        throw ApiError(400, "VALIDATION", "Committee name is too long (max 100 characters).");
    name_ = s;
}

void Committee::setAbbreviation(const std::string& v) {
    std::string s = validate::trim(v);
    if (s.empty()) throw ApiError(400, "VALIDATION", "Abbreviation can't be empty.");
    if (s.size() > DSA_SHORT_LEN - 1)
        throw ApiError(400, "VALIDATION", "Abbreviation is too long (max 20 characters).");
    abbreviation_ = s;
}

void Committee::setAgenda(const std::string& v) {
    std::string s = validate::trim(v);
    if (s.size() > DSA_TEXT_LEN - 1)
        throw ApiError(400, "VALIDATION", "Agenda is too long (max 300 characters).");
    agenda_ = s;
}

void Committee::setCapacity(int v) {
    if (v < kMinCapacity || v > kMaxCapacity)
        throw ApiError(400, "VALIDATION", "Capacity must be between " +
                                              std::to_string(kMinCapacity) + " and " +
                                              std::to_string(kMaxCapacity) + ".");
    capacity_ = v;
}

Json Committee::toJson() const {
    Json j = Json::object();
    j.set("id", id_)
        .set("conferenceId", conferenceId_)
        .set("name", name_)
        .set("abbreviation", abbreviation_)
        .set("agenda", agenda_)
        .set("capacity", capacity_);
    return j;
}

}  // namespace mun
