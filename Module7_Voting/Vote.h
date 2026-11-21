// MUNSphere — Module7_Voting/Vote.h
#pragma once

#include <string>

#include "Common.h"
#include "dsa_api.h"

namespace mun {

class Vote : public Entity {
public:
    explicit Vote(const VoteRec& rec);

    static std::string choiceKey(int c);           // yes|no|abstain
    static int parseChoice(const std::string& s);  // throws 400

    // The decision rule, kept in one place:
    //   simple majority : passes if YES > NO              (abstentions don't count)
    //   two-thirds      : passes if YES >= 2/3 of (YES + NO), and YES > 0
    static bool passes(int yes, int no, int majority);

    bool open() const { return rec_.state == VS_OPEN; }
    bool substantive() const { return rec_.substantive != 0; }
    const VoteRec& record() const { return rec_; }
    void checkChoiceAllowed(int choice) const;  // no abstaining on procedural votes
    void close(int yes, int no, int abstain);   // stores the tally and outcome

    // includeTally: open votes show live counts only to the EB and Admin
    Json toJson(bool includeTally) const;
    Json toJson() const override { return toJson(true); }
    std::string entityName() const override { return "Vote"; }

private:
    VoteRec rec_;
};

}  // namespace mun
