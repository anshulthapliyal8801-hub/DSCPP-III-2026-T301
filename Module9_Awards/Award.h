// MUNSphere — Module9_Awards/Award.h
// A ranking: entries built from marksheets, sorted with one of the five C
// algorithms, ranked with ties, and given awards by the configured rules.
#pragma once

#include <string>
#include <vector>

#include "Common.h"
#include "dsa_api.h"

namespace mun {

struct RankedRow {
    RankEntry entry;
    int rank = 0;              // competition ranking: 1, 2, 2, 4
    bool tied = false;         // same score, tie-break and attendance as a neighbour
    std::string separatedBy;   // what put this row below the one above
    std::string award;         // "" if none
};

class Ranking {
public:
    Ranking(std::vector<RankEntry> entries, int algorithm);

    // Sorts with the chosen algorithm and measures the real work done.
    void sortAndRank();
    void assignAwards(const std::vector<AwardRule>& rules);

    const std::vector<RankedRow>& rows() const { return rows_; }
    const SortStats& stats() const { return stats_; }
    long long elapsedNs() const { return elapsedNs_; }
    int algorithm() const { return algorithm_; }

    static int parseAlgorithm(const std::string& s);   // throws 400
    static std::string algorithmKey(int a);
    static bool sameStanding(const RankEntry& a, const RankEntry& b);  // tie ignoring ID

private:
    std::vector<RankEntry> input_;
    std::vector<RankedRow> rows_;
    int algorithm_;
    SortStats stats_{};
    long long elapsedNs_ = 0;
};

}  // namespace mun
