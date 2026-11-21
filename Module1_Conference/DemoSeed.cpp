// MUNSphere — Module1_Conference/DemoSeed.cpp
// `munsphere --seed-demo` fills an EMPTY data folder with a realistic
// conference for demonstrations and the viva. Added file: it drives every
// module, so it belongs with the program's entry point.
//
// Everything goes through the real Router with real logins, so every rule,
// permission and audit entry applies exactly as in normal use. The only
// shortcut is speech timing: real speeches take minutes, so finished
// speeches are logged through the C speaker list with simulated times.
#include "DemoSeed.h"

#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

#include "Database.h"
#include "dsa_api.h"

namespace mun {

namespace {

struct Seeder {
    Router& router;

    Json call(const std::string& method, const std::string& path, const std::string& token, const Json& body,
              bool mustSucceed = true) {
        HttpRequest req;
        req.method = method;
        req.path = path;
        if (!body.isNull()) req.body = body.dump();
        if (!token.empty()) req.headers["authorization"] = "Bearer " + token;
        HttpResponse res = router.dispatch(req);
        Json j = Json::parse(res.body);
        if (mustSucceed && res.status >= 400)
            throw std::runtime_error(method + " " + path + " failed: " + j["error"]["message"].asString());
        return j["data"];
    }
    std::string login(const std::string& id, const std::string& pw) {
        Json b = Json::object();
        b.set("id", id).set("password", pw);
        return call("POST", "/api/auth/login", "", b)["token"].asString();
    }
};

std::string dayOffset(int days) {
    std::time_t t = std::time(nullptr) + static_cast<std::time_t>(days) * 86400;
    std::tm tmv{};
#ifdef _WIN32
    tmv = *std::localtime(&t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", &tmv);
    return buf;
}

Json obj() { return Json::object(); }

}  // namespace

int seedDemo(Router& router, Auth& auth) {
    if (account_count() > 0 || conference_count() > 0) {
        std::fprintf(stderr, "The data folder isn't empty. Demo data can only be added to a new, empty data folder,\n"
                             "for example: build\\munsphere.exe --data demo-data --seed-demo\n");
        return 1;
    }
    Seeder s{router};
    const std::string adminPw = "Admin2026", delegatePw = "Delegate2026", boardPw = "Board2026";
    std::string today = dayOffset(0), tomorrow = dayOffset(1), dayAfter = dayOffset(2);
    try {
        std::printf("Seeding demo data...\n");
        auth.createAccount(ROLE_ADMIN, "Conference Admin", adminPw, false);
        std::string A = s.login("ADM001", adminPw);

        s.call("POST", "/api/conferences", A, obj().set("name", "Graphic Era MUN 2026")
            .set("venue", "Graphic Era University, Dehradun").set("startDate", today).set("endDate", dayAfter)
            .set("description", "Three committees over three days."));
        s.call("POST", "/api/committees", A, obj().set("conferenceId", "CONF001").set("name", "General Assembly")
            .set("abbreviation", "UNGA").set("capacity", 12).set("agenda", "Financing climate adaptation in developing states"));
        s.call("POST", "/api/committees", A, obj().set("conferenceId", "CONF001").set("name", "Security Council")
            .set("abbreviation", "UNSC").set("capacity", 10).set("agenda", "Maritime security in the Indo-Pacific"));
        s.call("POST", "/api/committees", A, obj().set("conferenceId", "CONF001").set("name", "All India Political Parties Meet")
            .set("abbreviation", "AIPPM").set("capacity", 12).set("agenda", "Electoral reform and simultaneous elections"));

        struct P { const char* name; const char* inst; const char* com; const char* portfolio; };
        const P people[] = {
            {"Rahul Sharma", "Graphic Era University", "COM001", "India"},
            {"Meera Iyer", "Doon University", "COM001", "Brazil"},
            {"Arjun Rao", "UPES", "COM001", "Japan"},
            {"Zoya Khan", "DIT University", "COM001", "Kenya"},
            {"Kabir Malhotra", "Graphic Era University", "COM001", "Germany"},
            {"Ananya Bisht", "Graphic Era University", "COM001", "Indonesia"},
            {"Vikram Negi", "UPES", "COM001", "Mexico"},
            {"Sana Qureshi", "Doon University", "COM001", "Egypt"},
            {"Aditya Rawat", "Graphic Era University", "COM002", "United States"},
            {"Ishita Joshi", "DIT University", "COM002", "China"},
            {"Rohan Kapoor", "UPES", "COM002", "France"},
            {"Nisha Thapa", "Graphic Era University", "COM002", "United Kingdom"},
            {"Farhan Ali", "Doon University", "COM002", "Russian Federation"},
            {"Pooja Semwal", "Graphic Era University", "COM002", "Ghana"},
            {"Dev Chauhan", "UPES", "COM002", "Ecuador"},
            {"Kavya Pant", "Graphic Era University", "COM003", "BJP"},
            {"Siddharth Rana", "DIT University", "COM003", "INC"},
            {"Tanvi Dobhal", "Doon University", "COM003", "AAP"},
            {"Aman Bhatt", "Graphic Era University", "COM003", "TMC"},
            {"Riya Uniyal", "UPES", "COM003", "DMK"},
            {"Harsh Panwar", "Graphic Era University", "COM003", "CPI(M)"},
            {"Neha Kandari", "DIT University", "COM003", "Shiv Sena (UBT)"},
            {"Karan Mehra", "UPES", "", ""},          // pending review
            {"Simran Gill", "Doon University", "", ""},  // rejected
        };
        std::vector<std::string> tokens;
        int i = 0;
        for (const P& p : people) {
            ++i;
            std::string email = "delegate" + std::to_string(i) + "@demo.munsphere";
            Json r = s.call("POST", "/api/auth/register", "", obj().set("name", p.name).set("email", email)
                .set("institution", p.inst).set("password", delegatePw));
            std::string tok = r["token"].asString(), id = r["user"]["id"].asString();
            tokens.push_back(tok);
            Json prefs = Json::array();
            prefs.push(p.com[0] ? p.com : "COM001");
            s.call("POST", "/api/delegate/me/registration", tok, obj().set("conferenceId", "CONF001").set("preferences", prefs));
            if (p.com[0]) {
                s.call("POST", "/api/delegates/" + id + "/approve", A, Json());
                s.call("POST", "/api/delegates/" + id + "/assign", A, obj().set("committeeId", p.com).set("portfolio", p.portfolio));
            } else if (i == 24) {
                s.call("POST", "/api/delegates/" + id + "/reject", A, obj().set("reason", "Registration fee not received."));
            }
        }

        struct E { const char* name; const char* role; const char* com; };
        const E board[] = {{"Priya Nair", "Chairperson", "COM001"}, {"Kabir Shah", "Moderator", "COM001"},
                           {"Sara Ali", "Scribe", "COM001"}, {"Devansh Malhotra", "Chairperson", "COM002"},
                           {"Ananya Gupta", "Moderator", "COM002"}, {"Rohan Verma", "Chairperson", "COM003"},
                           {"Isha Rawat", "Scribe", "COM003"}};
        std::vector<std::string> eb;
        for (const E& e : board) {
            Json r = s.call("POST", "/api/eb", A, obj().set("name", e.name).set("email", std::string(e.name).substr(0, std::string(e.name).find(' ')) + "@demo.munsphere")
                .set("ebRole", e.role).set("committeeId", e.com));
            std::string id = r["member"]["id"].asString(), temp = r["temporaryPassword"].asString();
            std::string t = s.login(id, temp);
            s.call("POST", "/api/auth/change-password", t, obj().set("currentPassword", temp).set("newPassword", boardPw));
            eb.push_back(t);
        }
        const std::string& UNGA_CHAIR = eb[0];
        const std::string& UNSC_CHAIR = eb[3];
        const std::string& AIPPM_CHAIR = eb[5];

        // Sessions: one finished and one live in UNGA; finished in UNSC; scheduled ahead.
        auto session = [&](const char* com, const char* title, const std::string& date, const char* a, const char* b) {
            return s.call("POST", "/api/sessions", A, obj().set("committeeId", com).set("title", title).set("date", date)
                .set("startTime", a).set("endTime", b))["id"].asString();
        };
        std::string u1 = session("COM001", "Opening session", today, "09:00", "10:30");
        std::string u2 = session("COM001", "General debate", today, "11:00", "13:00");
        std::string c1 = session("COM002", "Opening session", today, "09:00", "10:30");
        session("COM001", "Moderated caucus day", tomorrow, "10:00", "12:30");
        session("COM002", "Crisis briefing", tomorrow, "10:00", "12:00");
        session("COM003", "Opening session", tomorrow, "09:30", "11:30");
        session("COM003", "Voting procedure", dayAfter, "14:00", "16:00");

        long long now = nowEpoch();
        auto runFinishedSession = [&](const std::string& sid, const std::string& chair, int firstDelegate, int count) {
            s.call("POST", "/api/sessions/" + sid + "/activate", chair, Json());
            s.call("POST", "/api/sessions/" + sid + "/attendance", chair,
                   obj().set("delegateId", "D00" + std::to_string(firstDelegate + 1)).set("status", "late"));
            if (count > 6) {
                char last[16];
                std::snprintf(last, sizeof last, "D%03d", firstDelegate + count);
                s.call("POST", "/api/sessions/" + sid + "/attendance", chair, obj().set("delegateId", last).set("status", "absent"));
            }
            s.call("POST", "/api/sessions/" + sid + "/attendance/all", chair, obj().set("status", "present"));
            // Speeches with simulated durations (90 s allotted; one overruns).
            long long t = now - 3600;
            const int lengths[] = {84, 90, 71, 97, 62};
            for (int k = 0; k < 5; ++k) {
                char id[16];
                std::snprintf(id, sizeof id, "D%03d", firstDelegate + 1 + k);
                s.call("POST", "/api/sessions/" + sid + "/speakers", chair, obj().set("delegateId", id));
            }
            for (int k = 0; k < 5; ++k) {
                char ignored[DSA_ID_LEN];
                spk_next(sid.c_str(), t, ignored);
                t += lengths[k];
            }
            spk_finish(sid.c_str(), t);
            Database::instance().save(Store::Speakers);
        };
        runFinishedSession(u1, UNGA_CHAIR, 0, 8);
        runFinishedSession(c1, UNSC_CHAIR, 8, 7);

        // UNGA opening: a POI each way, a moderated caucus that passed, a suspension that failed.
        s.call("POST", "/api/sessions/" + u1 + "/speakers", UNGA_CHAIR, obj().set("delegateId", "D006"));
        char floor[DSA_ID_LEN];
        spk_next(u1.c_str(), now - 1800, floor);   // Indonesia takes the floor (simulated time)
        Json poi1 = s.call("POST", "/api/sessions/" + u1 + "/pois", tokens[1], obj().set("text", "Would the delegate of Indonesia say how the adaptation fund would be governed?"));
        s.call("POST", "/api/pois/" + poi1["id"].asString() + "/accept", UNGA_CHAIR, Json());
        Json poi2 = s.call("POST", "/api/sessions/" + u1 + "/pois", tokens[6], obj().set("text", "Is the delegate aware the weather is nice today?"));
        s.call("POST", "/api/pois/" + poi2["id"].asString() + "/reject", UNGA_CHAIR, obj().set("reason", "Not a substantive question to the speaker."));
        spk_finish(u1.c_str(), now - 1800 + 88);
        Database::instance().save(Store::Speakers);
        s.call("POST", "/api/sessions/" + u1 + "/motions", tokens[0], obj().set("type", "moderated")
            .set("topic", "Sources of adaptation finance").set("totalMinutes", 15).set("speakingSeconds", 60));
        s.call("POST", "/api/sessions/" + u1 + "/motions", tokens[3], obj().set("type", "suspend"));
        auto voteMotion = [&](const std::string& sid, const std::string& chair, int yesUpTo, int from, int to) {
            std::string v = s.call("POST", "/api/sessions/" + sid + "/motions/next", chair, Json())["voteId"].asString();
            for (int k = from; k < to; ++k)
                s.call("POST", "/api/votes/" + v + "/ballot", tokens[static_cast<std::size_t>(k)],
                       obj().set("choice", k < yesUpTo ? "yes" : "no"), false);  // the absent delegate is refused
            s.call("POST", "/api/votes/" + v + "/close", chair, Json());
        };
        voteMotion(u1, UNGA_CHAIR, 2, 0, 8);   // suspension: 2 yes, rest no -> fails
        voteMotion(u1, UNGA_CHAIR, 6, 0, 8);   // moderated caucus: passes
        std::string dr = s.call("POST", "/api/sessions/" + u1 + "/votes", UNGA_CHAIR,
                                obj().set("subject", "Draft resolution 1.1 on adaptation finance").set("substantive", true).set("majority", "two_thirds"))["id"].asString();
        const char* choices[] = {"yes", "yes", "yes", "no", "yes", "abstain", "yes"};
        for (int k = 0; k < 7; ++k) s.call("POST", "/api/votes/" + dr + "/ballot", tokens[static_cast<std::size_t>(k)], obj().set("choice", choices[k]));
        s.call("POST", "/api/votes/" + dr + "/close", UNGA_CHAIR, Json());
        s.call("POST", "/api/sessions/" + u1 + "/complete", UNGA_CHAIR, Json());
        s.call("POST", "/api/sessions/" + c1 + "/complete", UNSC_CHAIR, Json());

        // UNGA general debate: LIVE now, with a speaker on the floor.
        s.call("POST", "/api/sessions/" + u2 + "/activate", UNGA_CHAIR, Json());
        s.call("POST", "/api/sessions/" + u2 + "/attendance/all", UNGA_CHAIR, obj().set("status", "present"));
        for (const char* d : {"D003", "D005", "D007", "D002"})
            s.call("POST", "/api/sessions/" + u2 + "/speakers", UNGA_CHAIR, obj().set("delegateId", d));
        {
            char ignored[DSA_ID_LEN];
            spk_next(u2.c_str(), nowEpoch() - 25, ignored);   // Japan has spoken for 25 s
            Database::instance().save(Store::Speakers);
        }
        s.call("POST", "/api/sessions/" + u2 + "/speak-requests", tokens[7], Json());
        s.call("POST", "/api/sessions/" + u2 + "/pois", tokens[0], obj().set("text", "Could the delegate of Japan clarify its position on loss and damage?"));
        s.call("POST", "/api/sessions/" + u2 + "/motions", tokens[3], obj().set("type", "unmoderated").set("totalMinutes", 10));
        s.call("POST", "/api/sessions/" + u2 + "/motions", tokens[5], obj().set("type", "speaking_time").set("speakingSeconds", 60));

        // Marksheets: UNGA published, UNSC awaiting review (one returned), AIPPM drafts.
        auto keys = std::vector<std::string>{"research", "speaking", "participation", "diplomacy", "poi_motion"};
        const int unga[8][5] = {{18, 17, 17, 13, 12}, {16, 18, 15, 12, 11}, {15, 14, 16, 11, 10}, {17, 15, 14, 13, 13},
                                {13, 12, 13, 10, 9}, {16, 16, 15, 12, 10}, {12, 13, 12, 9, 8}, {14, 13, 11, 10, 9}};
        auto writeSheet = [&](const std::string& chair, const char* com, int delegateNo, const int* m, const char* remarks) {
            char id[16];
            std::snprintf(id, sizeof id, "D%03d", delegateNo);
            std::string sid = s.call("POST", std::string("/api/committees/") + com + "/marksheets", chair, obj().set("delegateId", id))["id"].asString();
            Json marks = Json::object();
            for (int k = 0; k < 5; ++k) marks.set(keys[static_cast<std::size_t>(k)], m[k]);
            s.call("PUT", "/api/marksheets/" + sid, chair, obj().set("marks", marks).set("remarks", remarks));
            return sid;
        };
        for (int k = 0; k < 8; ++k) {
            std::string sid = writeSheet(UNGA_CHAIR, "COM001", k + 1, unga[k], k == 0 ? "Outstanding research and composure under POIs." : "Solid contribution; keep building alliances.");
            s.call("POST", "/api/marksheets/" + sid + "/submit", UNGA_CHAIR, Json());
            s.call("POST", "/api/marksheets/" + sid + "/approve", A, Json());
        }
        s.call("POST", "/api/committees/COM001/marksheets/publish", A, Json());
        for (int k = 0; k < 7; ++k) {
            int m[5] = {12 + k % 6, 13 + (k * 3) % 6, 14 + k % 5, 10 + k % 4, 9 + k % 5};
            std::string sid = writeSheet(UNSC_CHAIR, "COM002", 9 + k, m, "Clear positions throughout the opening session.");
            s.call("POST", "/api/marksheets/" + sid + "/submit", UNSC_CHAIR, Json());
            if (k == 2) s.call("POST", "/api/marksheets/" + sid + "/return", A, obj().set("note", "Diplomacy looks high for one speech; please check."));
        }
        for (int k = 0; k < 3; ++k) {
            int m[5] = {14, 13, 12, 11, 10};
            writeSheet(AIPPM_CHAIR, "COM003", 16 + k, m, "");
        }

        s.call("POST", "/api/announcements", A, obj().set("title", "Welcome to Graphic Era MUN 2026")
            .set("body", "Opening ceremony in the main auditorium. Committees begin at 9:00. Carry your ID card.").set("audience", "all").set("pinned", true));
        s.call("POST", "/api/announcements", A, obj().set("title", "Executive Board briefing")
            .set("body", "All chairs and moderators: 8:30 in the faculty lounge.").set("audience", "eb"));
        s.call("POST", "/api/announcements", A, obj().set("title", "AIPPM room change")
            .set("body", "AIPPM moves to Seminar Hall 2 from tomorrow.").set("audience", "committee").set("committeeId", "COM003"));
        s.call("POST", "/api/backups", A, obj().set("label", "Freshly seeded demo data"));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Seeding stopped: %s\n", e.what());
        return 1;
    }

    std::printf("\nDemo conference ready: Graphic Era MUN 2026 (%s to %s)\n\n", today.c_str(), dayAfter.c_str());
    std::printf("  Admin              ADM001          password %s\n", adminPw.c_str());
    std::printf("  UNGA chair         EB001           password %s   (Moderator EB002, Scribe EB003)\n", boardPw.c_str());
    std::printf("  UNSC chair         EB004           password %s   (Moderator EB005)\n", boardPw.c_str());
    std::printf("  AIPPM chair        EB006           password %s   (Scribe EB007)\n", boardPw.c_str());
    std::printf("  Delegates          D001 to D024    password %s\n", delegatePw.c_str());
    std::printf("                     D001 India (UNGA, Best Delegate), D009 United States (UNSC),\n");
    std::printf("                     D016 BJP (AIPPM), D023 pending review, D024 rejected\n\n");
    std::printf("UNGA's general debate is live now. Start the server and log in.\n");
    return 0;
}

}  // namespace mun
