// MUNSphere — Module7_Voting/Vote.cpp
#include "Vote.h"

namespace mun {

Vote::Vote(const VoteRec& rec) : Entity(rec.id), rec_(rec) {}

std::string Vote::choiceKey(int c) {
    switch (c) {
    case VOTE_YES: return "yes";
    case VOTE_NO: return "no";
    case VOTE_ABSTAIN: return "abstain";
    default: return "unknown";
    }
}

int Vote::parseChoice(const std::string& s) {
    for (int c = 0; c < VOTE_CHOICE_COUNT; ++c)
        if (s == choiceKey(c)) return c;
    throw ApiError(400, "VALIDATION", "Vote yes, no or abstain.");
}

bool Vote::passes(int yes, int no, int majority) {
    if (majority == MAJ_TWO_THIRDS) return yes > 0 && 3 * yes >= 2 * (yes + no);
    return yes > no;
}

void Vote::checkChoiceAllowed(int choice) const {
    if (choice == VOTE_ABSTAIN && !substantive())
        throw ApiError(400, "NO_ABSTAIN", "Abstentions aren't allowed on procedural votes.");
}

void Vote::close(int yes, int no, int abstain) {
    if (!open()) throw ApiError(409, "INVALID_STATE", "This vote is already closed.");
    rec_.state = VS_CLOSED;
    rec_.yes = yes;
    rec_.no = no;
    rec_.abstain = abstain;
    rec_.outcome = passes(yes, no, rec_.majority) ? 1 : 0;
    pers_copy_field(rec_.closedAt, nowTimestamp().c_str(), DSA_STAMP_LEN);
}

Json Vote::toJson(bool includeTally) const {
    int counts[VOTE_CHOICE_COUNT] = {rec_.yes, rec_.no, rec_.abstain};
    int cast = open() ? vote_tally(rec_.id, counts) : rec_.yes + rec_.no + rec_.abstain;
    Json j = Json::object();
    j.set("id", rec_.id)
        .set("sessionId", rec_.sessionId)
        .set("subject", rec_.subject)
        .set("motionId", rec_.motionId)
        .set("kind", substantive() ? "substantive" : "procedural")
        .set("majority", rec_.majority == MAJ_TWO_THIRDS ? "two_thirds" : "simple")
        .set("state", open() ? "open" : "closed")
        .set("ballotsCast", cast)
        .set("openedAt", rec_.openedAt)
        .set("closedAt", rec_.closedAt);
    if (includeTally || !open()) {
        Json t = Json::object();
        t.set("yes", counts[0]).set("no", counts[1]).set("abstain", counts[2]);
        Json pct = Json::object();
        for (int c = 0; c < VOTE_CHOICE_COUNT; ++c)
            pct.set(choiceKey(c), cast ? (counts[c] * 1000 / cast) / 10.0 : 0.0);
        j.set("tally", t).set("percentages", pct);
    }
    if (!open()) j.set("outcome", rec_.outcome == 1 ? "passed" : "failed");
    return j;
}

}  // namespace mun
