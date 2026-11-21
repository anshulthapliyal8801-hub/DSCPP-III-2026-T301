// MUNSphere — Module9_Awards/Award.cpp
#include "Award.h"

#include <chrono>

namespace mun {

namespace {
const char* kKeys[SORT_ALGORITHM_COUNT] = {"bubble", "selection", "insertion", "merge", "quick"};
}

Ranking::Ranking(std::vector<RankEntry> entries, int algorithm) : input_(std::move(entries)), algorithm_(algorithm) {}

int Ranking::parseAlgorithm(const std::string& s) {
    if (s.empty()) return SORT_MERGE;
    for (int a = 0; a < SORT_ALGORITHM_COUNT; ++a)
        if (s == kKeys[a]) return a;
    throw ApiError(400, "VALIDATION", "Algorithm must be bubble, selection, insertion, merge or quick.");
}

std::string Ranking::algorithmKey(int a) { return a >= 0 && a < SORT_ALGORITHM_COUNT ? kKeys[a] : "unknown"; }

bool Ranking::sameStanding(const RankEntry& a, const RankEntry& b) {
    return a.percentTenths == b.percentTenths && a.tiebreakTenths == b.tiebreakTenths &&
           a.attendanceTenths == b.attendanceTenths;
}

void Ranking::sortAndRank() {
    std::vector<RankEntry> a = input_;
    auto t0 = std::chrono::steady_clock::now();
    rank_sort(a.data(), static_cast<int>(a.size()), algorithm_, &stats_);   // the C algorithm
    elapsedNs_ = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();

    rows_.clear();
    for (std::size_t i = 0; i < a.size(); ++i) {
        RankedRow r;
        r.entry = a[i];
        if (i == 0) {
            r.rank = 1;
        } else {
            const RankEntry& prev = a[i - 1];
            if (sameStanding(prev, a[i])) {
                r.rank = rows_.back().rank;   // genuine tie: shared rank
                r.tied = true;
                rows_.back().tied = true;
                r.separatedBy = "tied";
            } else {
                r.rank = static_cast<int>(i) + 1;
                r.separatedBy = prev.percentTenths != a[i].percentTenths ? "score"
                                : prev.tiebreakTenths != a[i].tiebreakTenths ? "tie-break criterion"
                                                                             : "attendance";
            }
        }
        rows_.push_back(r);
    }
}

// Awards go down the ranking in rule order. A rule takes up to
// `positions` delegates who reach its minimum; delegates tied with the
// last one taken share the award. Someone below a rule's minimum can still
// qualify for a later rule with a lower minimum.
void Ranking::assignAwards(const std::vector<AwardRule>& rules) {
    std::size_t i = 0;
    for (const AwardRule& rule : rules) {
        int given = 0;
        while (i < rows_.size() && given < rule.positions && rows_[i].entry.percentTenths >= rule.minPercent * 10) {
            rows_[i].award = rule.name;
            ++given;
            ++i;
            while (i < rows_.size() && sameStanding(rows_[i - 1].entry, rows_[i].entry)) {  // shared
                rows_[i].award = rule.name;
                ++i;
            }
        }
    }
}

}  // namespace mun
