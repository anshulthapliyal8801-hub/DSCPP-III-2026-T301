// MUNSphere — Module11_Reports/Report.cpp
#include "Report.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

#include "DelegateManager.h"
#include "Motion.h"
#include "Score.h"
#include "Vote.h"

namespace mun {

std::string fmt1(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", v);
    return b;
}

namespace {
std::string str(long long v) { return std::to_string(v); }

std::string committeeAbbr(const std::string& id) {
    CommitteeRec c;
    return committee_find(id.c_str(), &c, nullptr) == DSA_OK ? std::string(c.abbreviation) : id;
}
std::string committeeOfSession(const std::string& id) {
    SessionRec s;
    return session_find(id.c_str(), &s, nullptr) == DSA_OK ? std::string(s.committeeId) : std::string();
}
std::string nameOf(const std::string& delegateId) {
    Json b = delegateBrief(delegateId);
    return b.isNull() ? delegateId : b["name"].asString();
}
std::string portfolioOf(const std::string& delegateId) {
    Json b = delegateBrief(delegateId);
    return b.isNull() ? "" : b["portfolio"].asString();
}
std::vector<DelegateRec> allDelegates() {
    std::vector<DelegateRec> v(static_cast<std::size_t>(delegate_count()));
    v.resize(static_cast<std::size_t>(delegate_to_array(v.data(), static_cast<int>(v.size()))));
    return v;
}
std::vector<SessionRec> allSessions() {
    std::vector<SessionRec> v(static_cast<std::size_t>(session_count()));
    v.resize(static_cast<std::size_t>(session_to_array(v.data(), static_cast<int>(v.size()))));
    return v;
}
// Score % from an approved or published marksheet; "" otherwise.
std::string confirmedScore(const std::string& delegateId) {
    MarksheetRec m;
    if (sheet_find_by_delegate(delegateId.c_str(), &m) != DSA_OK) return "";
    if (m.status != SHEET_APPROVED && m.status != SHEET_PUBLISHED) return "";
    return fmt1(Marksheet(m).percent());
}
void addStats(ReportTable& t, const std::vector<int>& tenths, const std::string& what) {
    StatSummary s;
    stats_summary(tenths.data(), static_cast<int>(tenths.size()), &s);   // computed in C
    if (!s.count) return;
    t.summary.push_back({what + " mean", fmt1(s.meanTenths / 100.0)});
    t.summary.push_back({what + " median", fmt1(s.medianTenths / 100.0)});
    t.summary.push_back({what + " standard deviation", fmt1(s.stddevTenths / 100.0)});
    t.summary.push_back({what + " range", fmt1(s.min / 10.0) + " to " + fmt1(s.max / 10.0)});
}

// ------------------------------------------------------------ reports

class ConferenceReport : public Report {
public:
    std::string key() const override { return "conference"; }
    std::string title() const override { return "Conference summary"; }
    std::string description() const override { return "Every committee: places, Executive Board, sessions, attendance and published results."; }
    ReportTable build(const ReportContext& c) const override {
        ReportTable t;
        t.columns = {"Committee", "Conference", "Capacity", "Placed", "EB", "Sessions held", "Scheduled", "Attendance %", "Published"};
        int placedAll = 0, held = 0;
        for (const Committee& com : c.conf.listCommittees("", "")) {
            int placed = static_cast<int>(delegatesInCommittee(com.id()).size()), h = 0, sch = 0, att = 0, slots = 0;
            for (const Session& s : c.conf.listSessions(com.id(), "")) {
                if (s.status() == SESSION_SCHEDULED) { sch++; continue; }
                h++;
                int counts[ATT_STATUS_COUNT];
                att_session_tally(s.id().c_str(), counts);
                att += counts[ATT_PRESENT] + counts[ATT_LATE];
                slots += placed;
            }
            MarksheetRec buf[512];
            int pub = sheet_list(com.id().c_str(), SHEET_PUBLISHED, buf, 512);
            ConferenceRec conf;
            conference_find(com.conferenceId().c_str(), &conf, nullptr);
            t.rows.push_back({com.abbreviation(), conf.name, str(com.capacity()), str(placed), str(eb_count_by_committee(com.id().c_str())),
                              str(h), str(sch), slots ? fmt1(att * 100.0 / slots) : "", str(pub)});
            placedAll += placed;
            held += h;
        }
        t.summary = {{"Conferences", str(conference_count())}, {"Committees", str(committee_count())},
                     {"Delegate accounts", str(delegate_count())}, {"Delegates placed", str(placedAll)},
                     {"Sessions held", str(held)}, {"Executive Board members", str(eb_count())}};
        return t;
    }
};

class DelegatesReport : public Report {
public:
    std::string key() const override { return "delegates"; }
    std::string title() const override { return "Delegates"; }
    std::string description() const override { return "Every delegate with registration status, placement, attendance and confirmed score."; }
    ReportTable build(const ReportContext& c) const override {
        ReportTable t;
        t.columns = {"ID", "Name", "Institution", "Status", "Committee", "Portfolio", "Attendance %", "Score %"};
        std::map<std::string, int> byStatus;
        std::vector<DelegateRec> all = allDelegates();
        std::sort(all.begin(), all.end(), [](const DelegateRec& a, const DelegateRec& b) { return std::string(a.id) < b.id; });
        for (const DelegateRec& d : all) {
            if (!c.committeeId.empty() && c.committeeId != d.committeeId) continue;
            std::string status = d.committeeId[0] ? "placed" : Delegate::statusKey(d.status);
            byStatus[status]++;
            std::string att = d.committeeId[0] ? fmt1(c.att.delegateSummary(d.id)["percentage"].asNumber()) : "";
            t.rows.push_back({d.id, d.name, d.institution, status, d.committeeId[0] ? committeeAbbr(d.committeeId) : "",
                              d.portfolio, att, confirmedScore(d.id)});
        }
        for (const auto& kv : byStatus) t.summary.push_back({"Status: " + kv.first, str(kv.second)});
        return t;
    }
};

class CommitteeReport : public Report {
public:
    std::string key() const override { return "committee"; }
    std::string title() const override { return "Committee participation"; }
    std::string description() const override { return "One committee: each delegate's attendance, speeches, POIs, motions, votes and score."; }
    bool committeeScoped() const override { return true; }
    bool requiresCommittee() const override { return true; }
    ReportTable build(const ReportContext& c) const override {
        Committee com = c.conf.getCommittee(c.committeeId);
        ReportTable t;
        t.columns = {"Portfolio", "Delegate", "Attendance %", "Speeches", "Speaking time (s)", "POIs raised", "POIs accepted",
                     "Motions proposed", "Ballots cast", "Score %"};
        std::vector<SpeechRec> speeches(static_cast<std::size_t>(speech_count()) + 1);
        int ns = speech_list(nullptr, nullptr, speeches.data(), static_cast<int>(speeches.size()));
        std::vector<PoiRec> pois(static_cast<std::size_t>(poi_count()) + 1);
        int np = poi_list(nullptr, nullptr, pois.data(), static_cast<int>(pois.size()));
        std::vector<MotionRec> motions(2048);
        int nm = motion_list(nullptr, motions.data(), 2048);
        std::vector<VoteRec> votes(1024);
        int nv = vote_list(nullptr, votes.data(), 1024);
        for (const DelegateRec& d : delegatesInCommittee(com.id())) {
            int sp = 0, secs = 0, pr = 0, pa = 0, mo = 0, bc = 0;
            for (int i = 0; i < ns; ++i)
                if (!std::strcmp(speeches[static_cast<std::size_t>(i)].delegateId, d.id)) { sp++; secs += speeches[static_cast<std::size_t>(i)].seconds; }
            for (int i = 0; i < np; ++i)
                if (!std::strcmp(pois[static_cast<std::size_t>(i)].delegateId, d.id)) { pr++; if (pois[static_cast<std::size_t>(i)].status == POI_ACCEPTED) pa++; }
            for (int i = 0; i < nm; ++i)
                if (!std::strcmp(motions[static_cast<std::size_t>(i)].proposerId, d.id)) mo++;
            for (int i = 0; i < nv; ++i)
                if (ballot_find(votes[static_cast<std::size_t>(i)].id, d.id, nullptr, nullptr) == DSA_OK) bc++;
            t.rows.push_back({d.portfolio, d.name, fmt1(c.att.delegateSummary(d.id)["percentage"].asNumber()), str(sp), str(secs),
                              str(pr), str(pa), str(mo), str(bc), confirmedScore(d.id)});
        }
        int held = 0;
        for (const Session& s : c.conf.listSessions(com.id(), "")) if (s.status() != SESSION_SCHEDULED) held++;
        t.summary = {{"Committee", com.name() + " (" + com.abbreviation() + ")"}, {"Agenda", com.agenda()},
                     {"Delegates placed", str(static_cast<long long>(t.rows.size())) + " of " + str(com.capacity())},
                     {"Sessions held", str(held)}};
        return t;
    }
};

class AttendanceReport : public Report {
public:
    std::string key() const override { return "attendance"; }
    std::string title() const override { return "Attendance"; }
    std::string description() const override { return "Every session that has taken place, with present, late, absent and unmarked counts."; }
    bool committeeScoped() const override { return true; }
    ReportTable build(const ReportContext& c) const override {
        ReportTable t;
        t.columns = {"Session", "Committee", "Date", "Present", "Late", "Absent", "Checked in", "Not marked", "Attendance %"};
        long long attended = 0, slots = 0;
        for (const SessionRec& s : allSessions()) {
            if (s.status == SESSION_SCHEDULED) continue;
            if (!c.committeeId.empty() && c.committeeId != s.committeeId) continue;
            int counts[ATT_STATUS_COUNT];
            int marked = att_session_tally(s.id, counts);
            int roster = static_cast<int>(delegatesInCommittee(s.committeeId).size());
            int unmarked = roster > marked ? roster - marked : 0;
            int a = counts[ATT_PRESENT] + counts[ATT_LATE];
            attended += a;
            slots += roster;
            t.rows.push_back({std::string(s.id) + " " + s.title, committeeAbbr(s.committeeId), s.date, str(counts[ATT_PRESENT]),
                              str(counts[ATT_LATE]), str(counts[ATT_ABSENT]), str(counts[ATT_CHECKED_IN]), str(unmarked),
                              roster ? fmt1(a * 100.0 / roster) : ""});
        }
        t.summary = {{"Sessions", str(static_cast<long long>(t.rows.size()))},
                     {"Overall attendance %", slots ? fmt1(attended * 100.0 / slots) : "n/a"},
                     {"Rule", "Late counts as attending; absent and unmarked don't."}};
        return t;
    }
};

class SpeakersReport : public Report {
public:
    std::string key() const override { return "speakers"; }
    std::string title() const override { return "Speakers"; }
    std::string description() const override { return "Speeches per delegate from the speaker list history, with time used against time given."; }
    bool committeeScoped() const override { return true; }
    ReportTable build(const ReportContext& c) const override {
        ReportTable t;
        t.columns = {"Delegate", "Portfolio", "Committee", "Speeches", "Total time (s)", "Average (s)", "Over time"};
        std::vector<SpeechRec> sp(static_cast<std::size_t>(speech_count()) + 1);
        int n = speech_list(nullptr, nullptr, sp.data(), static_cast<int>(sp.size()));
        std::map<std::string, std::vector<SpeechRec>> by;
        std::vector<int> lengthsX10;
        for (int i = 0; i < n; ++i) {
            const SpeechRec& s = sp[static_cast<std::size_t>(i)];
            std::string com = committeeOfSession(s.sessionId);
            if (!c.committeeId.empty() && com != c.committeeId) continue;
            by[s.delegateId].push_back(s);
            lengthsX10.push_back(s.seconds * 10);
        }
        for (const auto& kv : by) {
            int total = 0, over = 0;
            for (const SpeechRec& s : kv.second) { total += s.seconds; if (s.seconds > s.allotted) over++; }
            t.rows.push_back({nameOf(kv.first), portfolioOf(kv.first), committeeAbbr(committeeOfSession(kv.second.front().sessionId)),
                              str(static_cast<long long>(kv.second.size())), str(total),
                              fmt1(static_cast<double>(total) / static_cast<double>(kv.second.size())), str(over)});
        }
        t.summary.push_back({"Speeches", str(static_cast<long long>(lengthsX10.size()))});
        t.summary.push_back({"Delegates who spoke", str(static_cast<long long>(by.size()))});
        addStats(t, lengthsX10, "Speech length (s)");
        return t;
    }
};

class MotionsReport : public Report {
public:
    std::string key() const override { return "motions"; }
    std::string title() const override { return "Motions"; }
    std::string description() const override { return "Every motion with its type, proposer, outcome and vote."; }
    bool committeeScoped() const override { return true; }
    ReportTable build(const ReportContext& c) const override {
        ReportTable t;
        t.columns = {"ID", "Session", "Committee", "Motion", "Proposed by", "Status", "Vote"};
        std::vector<MotionRec> m(2048);
        int n = motion_list(nullptr, m.data(), 2048);
        std::map<std::string, int> byStatus;
        for (int i = n - 1; i >= 0; --i) {  // oldest first
            const MotionRec& r = m[static_cast<std::size_t>(i)];
            if (!c.committeeId.empty() && c.committeeId != r.committeeId) continue;
            Motion mo(r);
            std::string vote;
            VoteRec v;
            if (r.voteId[0] && vote_find(r.voteId, &v) == DSA_OK && v.state == VS_CLOSED)
                vote = str(v.yes) + " yes, " + str(v.no) + " no";
            byStatus[Motion::statusKey(r.status)]++;
            t.rows.push_back({r.id, r.sessionId, committeeAbbr(r.committeeId), mo.describe(), portfolioOf(r.proposerId),
                              Motion::statusKey(r.status), vote});
        }
        for (const auto& kv : byStatus) t.summary.push_back({"Status: " + kv.first, str(kv.second)});
        return t;
    }
};

class VotingReport : public Report {
public:
    std::string key() const override { return "voting"; }
    std::string title() const override { return "Voting"; }
    std::string description() const override { return "Every closed vote with counts, percentages, outcome and ballot corrections."; }
    bool committeeScoped() const override { return true; }
    ReportTable build(const ReportContext& c) const override {
        ReportTable t;
        t.columns = {"ID", "Session", "Subject", "Kind", "Majority", "Yes", "No", "Abstain", "Yes %", "Outcome", "Corrections"};
        std::vector<VoteRec> v(1024);
        int n = vote_list(nullptr, v.data(), 1024), passed = 0, failed = 0;
        for (int i = n - 1; i >= 0; --i) {
            const VoteRec& r = v[static_cast<std::size_t>(i)];
            if (r.state != VS_CLOSED) continue;
            if (!c.committeeId.empty() && c.committeeId != r.committeeId) continue;
            BallotRec b[512];
            int nb = ballot_list(r.id, b, 512), corr = 0;
            for (int k = 0; k < nb; ++k) corr += b[k].corrections;
            int total = r.yes + r.no + r.abstain;
            (r.outcome == 1 ? passed : failed)++;
            t.rows.push_back({r.id, r.sessionId, r.subject, r.substantive ? "substantive" : "procedural",
                              r.majority == MAJ_TWO_THIRDS ? "two-thirds" : "simple", str(r.yes), str(r.no), str(r.abstain),
                              total ? fmt1(r.yes * 100.0 / total) : "", r.outcome == 1 ? "passed" : "failed", str(corr)});
        }
        t.summary = {{"Votes held", str(passed + failed)}, {"Passed", str(passed)}, {"Failed", str(failed)}};
        return t;
    }
};

class ScoringReport : public Report {
public:
    std::string key() const override { return "scoring"; }
    std::string title() const override { return "Scoring"; }
    std::string description() const override { return "Every marksheet with its status and score, the score distribution, and statistics."; }
    ReportTable build(const ReportContext& c) const override {
        ReportTable t;
        t.columns = {"Marksheet", "Delegate", "Portfolio", "Committee", "Status", "Marks", "Out of", "Score %"};
        std::vector<MarksheetRec> v(static_cast<std::size_t>(sheet_count()) + 1);
        int n = sheet_list(c.committeeId.c_str(), -1, v.data(), static_cast<int>(v.size()));
        std::vector<int> confirmed;
        for (int i = 0; i < n; ++i) {
            const MarksheetRec& r = v[static_cast<std::size_t>(i)];
            Marksheet m(r);
            t.rows.push_back({r.id, nameOf(r.delegateId), portfolioOf(r.delegateId), committeeAbbr(r.committeeId),
                              Marksheet::statusKey(r.status), str(r.total), str(r.maxTotal), fmt1(m.percent())});
            if (r.status == SHEET_APPROVED || r.status == SHEET_PUBLISHED)
                confirmed.push_back(r.maxTotal ? static_cast<int>(std::lround(r.total * 1000.0 / r.maxTotal)) : 0);
        }
        t.summary.push_back({"Marksheets", str(n)});
        t.summary.push_back({"Approved or published", str(static_cast<long long>(confirmed.size()))});
        addStats(t, confirmed, "Confirmed score %");
        int buckets[10];
        stats_histogram(confirmed.data(), static_cast<int>(confirmed.size()), 0, 100, buckets, 10);  // 10% bands, in C
        Json labels = Json::array(), values = Json::array();
        for (int b = 0; b < 10; ++b) {
            labels.push(str(b * 10) + (b == 9 ? "–100" : "–" + str(b * 10 + 9)));
            values.push(buckets[b]);
        }
        t.chart = Json::object();
        t.chart.set("title", "Confirmed scores by 10% band").set("labels", labels).set("values", values);
        return t;
    }
};

class AwardsReport : public Report {
public:
    std::string key() const override { return "awards"; }
    std::string title() const override { return "Awards"; }
    std::string description() const override { return "Award winners in every committee, from approved and published marksheets."; }
    ReportTable build(const ReportContext& c) const override {
        ReportTable t;
        t.columns = {"Committee", "Award", "Rank", "Delegate", "Portfolio", "Score %"};
        AuthContext admin;
        admin.role = ROLE_ADMIN;
        int count = 0;
        for (const Committee& com : c.conf.listCommittees("", "")) {
            if (!c.committeeId.empty() && c.committeeId != com.id()) continue;
            Json r = c.ranking.committeeRanking(admin, com.id(), "merge", "approved");
            for (const Json& row : r["rows"].asArray()) {
                if (row["award"].isNull()) continue;
                t.rows.push_back({com.abbreviation(), row["award"].asString(), str(static_cast<long long>(row["rank"].asNumber())) + (row["tied"].asBool() ? "=" : ""),
                                  row["delegate"]["name"].asString(), row["delegate"]["portfolio"].asString(), fmt1(row["percent"].asNumber())});
                count++;
            }
        }
        t.summary = {{"Awards given", str(count)}, {"Ranking order", "Score, tie-break criterion, attendance, then Delegate ID"}};
        return t;
    }
};

class DsaReport : public Report {
public:
    std::string key() const override { return "dsa"; }
    std::string title() const override { return "Data structure performance"; }
    std::string description() const override { return "Measured comparisons for each search over every delegate, and the shape of each structure."; }
    ReportTable build(const ReportContext&) const override {
        ReportTable t;
        t.columns = {"Structure or algorithm", "Module", "n", "Measure", "Value"};
        std::vector<DelegateRec> dels = allDelegates();
        int n = static_cast<int>(dels.size());
        struct M { const char* name; long long total = 0; int max = 0; long long ns = 0; };
        M ms[4] = {{"Hash table lookup by ID"}, {"Linear search by ID"}, {"Binary search by ID"}, {"BST search by name"}};
        for (const DelegateRec& d : dels) {
            SearchTrace tr;
            for (int k = 0; k < 4; ++k) {
                auto t0 = std::chrono::steady_clock::now();
                if (k == 0) delegate_search_hash(d.id, nullptr, &tr);
                else if (k == 1) delegate_search_linear_id(d.id, nullptr, &tr);
                else if (k == 2) delegate_search_binary_id(d.id, nullptr, &tr);
                else delegate_search_bst_name(d.name, nullptr, &tr);
                ms[k].ns += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
                ms[k].total += tr.comparisons;
                if (tr.comparisons > ms[k].max) ms[k].max = tr.comparisons;
            }
        }
        for (const M& m : ms) {
            t.rows.push_back({m.name, "2", str(n), "Average comparisons", n ? fmt1(static_cast<double>(m.total) / n) : "0"});
            t.rows.push_back({m.name, "2", str(n), "Most comparisons", str(m.max)});
        }
        int longest = 0;
        for (int b = 0; b < delegate_bucket_count(); ++b) longest = std::max(longest, delegate_bucket_length(b));
        int ideal = 0;
        while ((1 << ideal) - 1 < n) ideal++;
        t.rows.push_back({"Delegate hash table", "2", str(n), "Buckets / load factor", str(delegate_bucket_count()) + " / " + fmt1(delegate_load_factor())});
        t.rows.push_back({"Delegate hash table", "2", str(n), "Longest chain / resizes", str(longest) + " / " + str(delegate_rehash_count())});
        t.rows.push_back({"Delegate BST", "2", str(n), "Height (minimum possible)", str(bst_height()) + " (" + str(ideal) + ")"});
        t.rows.push_back({"Conference / committee / session lists", "1", str(conference_count() + committee_count() + session_count()),
                          "Lengths", str(conference_count()) + " / " + str(committee_count()) + " / " + str(session_count())});
        t.rows.push_back({"Account hash table", "Shared", str(account_count()), "Buckets", "101 (fixed)"});
        t.rows.push_back({"EB adjacency list", "10", str(eb_count()), "Committee vertices", str(eb_committee_count())});
        t.summary = {{"Delegates searched", str(n)},
                     {"Note", "Every delegate is looked up once by each method. Linear and binary search times include copying (and, for binary search, sorting) the array; their comparison counts don't."}};
        return t;
    }
};

}  // namespace

// ------------------------------------------------------------- rendering

Json Report::toJson(const ReportTable& t, const std::string& scope) const {
    Json cols = Json::array(), rows = Json::array(), sum = Json::array();
    for (const std::string& c : t.columns) cols.push(c);
    for (const auto& r : t.rows) {
        Json row = Json::array();
        for (const std::string& v : r) row.push(v);
        rows.push(row);
    }
    for (const auto& s : t.summary) {
        Json j = Json::object();
        j.set("label", s.first).set("value", s.second);
        sum.push(j);
    }
    Json out = Json::object();
    out.set("key", key()).set("title", title()).set("description", description()).set("scope", scope)
        .set("generatedAt", nowTimestamp()).set("columns", cols).set("rows", rows).set("summary", sum).set("chart", t.chart);
    return out;
}

std::string Report::toCsv(const ReportTable& t) {
    auto cell = [](const std::string& v) {
        std::string out(v.size() * 2 + 3, '\0');
        int n = csv_escape(v.c_str(), &out[0], out.size());   // RFC 4180 quoting, in C
        out.resize(n < 0 ? 0 : static_cast<std::size_t>(n));
        return out;
    };
    std::string s;
    for (std::size_t i = 0; i < t.columns.size(); ++i) s += (i ? "," : "") + cell(t.columns[i]);
    s += "\r\n";
    for (const auto& r : t.rows) {
        for (std::size_t i = 0; i < r.size(); ++i) s += (i ? "," : "") + cell(r[i]);
        s += "\r\n";
    }
    return s;
}

namespace {
// Display width in characters, counting UTF-8 continuation bytes as zero.
std::size_t width(const std::string& s) {
    std::size_t w = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) w++;
    return w;
}
bool numeric(const std::string& s) {
    return !s.empty() && s.find_first_not_of("0123456789.-/ =") == std::string::npos;
}
}  // namespace

std::string Report::toText(const ReportTable& t, const std::string& scope) const {
    std::vector<std::size_t> w(t.columns.size());
    for (std::size_t i = 0; i < t.columns.size(); ++i) w[i] = width(t.columns[i]);
    for (const auto& r : t.rows)
        for (std::size_t i = 0; i < r.size() && i < w.size(); ++i) w[i] = std::max(w[i], std::min<std::size_t>(width(r[i]), 40));
    auto pad = [](std::string v, std::size_t n, bool right) {
        if (width(v) > n) { v = v.substr(0, n - 1) + "~"; }
        std::string sp(n - std::min(n, width(v)), ' ');
        return right ? sp + v : v + sp;
    };
    std::string s = "MUNSphere: " + title() + "\n" + "Scope: " + scope + "    Generated: " + nowTimestamp() + "\n\n";
    for (const auto& kv : t.summary) s += "  " + kv.first + ": " + kv.second + "\n";
    if (!t.summary.empty()) s += "\n";
    std::string rule;
    for (std::size_t i = 0; i < w.size(); ++i) {
        s += (i ? "  " : "") + pad(t.columns[i], w[i], false);
        rule += (i ? "  " : "") + std::string(w[i], '-');
    }
    s += "\n" + rule + "\n";
    for (const auto& r : t.rows) {
        for (std::size_t i = 0; i < r.size() && i < w.size(); ++i) s += (i ? "  " : "") + pad(r[i], w[i], numeric(r[i]));
        s += "\n";
    }
    if (t.rows.empty()) s += "(no rows)\n";
    return s;
}

std::vector<std::unique_ptr<Report>> makeReports() {
    std::vector<std::unique_ptr<Report>> v;
    v.push_back(std::make_unique<ConferenceReport>());
    v.push_back(std::make_unique<DelegatesReport>());
    v.push_back(std::make_unique<CommitteeReport>());
    v.push_back(std::make_unique<AttendanceReport>());
    v.push_back(std::make_unique<SpeakersReport>());
    v.push_back(std::make_unique<MotionsReport>());
    v.push_back(std::make_unique<VotingReport>());
    v.push_back(std::make_unique<ScoringReport>());
    v.push_back(std::make_unique<AwardsReport>());
    v.push_back(std::make_unique<DsaReport>());
    return v;
}

}  // namespace mun
