// MUNSphere — Module1_Conference/main.cpp
// Entry point of the single integrated MUNSphere server.
//
// It lives in Module1_Conference because the project structure places the
// original server here; it starts the WHOLE application.
//
// Usage (from the project root):
//   build\munsphere.exe [--port 8080] [--host 127.0.0.1] [--data data]
//   build\munsphere.exe --create-admin      controlled Admin account setup
//   build\munsphere.exe --data demo-data --seed-demo   demo conference in its own folder
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "API.h"
#include "Auth.h"
#include "CommitteeManager.h"
#include "Conference.h"
#include "Database.h"
#include "DelegateManager.h"
#include "EBManager.h"
#include "AttendanceManager.h"
#include "LiveSession.h"
#include "MotionManager.h"
#include "RankingManager.h"
#include "AuditManager.h"
#include "DemoSeed.h"
#include "PersistenceManager.h"
#include "ReportManager.h"
#include "ScoringManager.h"
#include "SpeakerManager.h"
#include "VotingManager.h"
#include "server.h"

using namespace mun;

namespace {

struct Config {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string dataDir = "data";
    std::string frontendDir = "frontend";
    std::string module1Dir = "Module1_Conference/frontend";
    std::string module2Dir = "Module2_Delegate/frontend";
    bool createAdmin = false;
    bool seedDemo = false;
};

bool parseArgs(int argc, char** argv, Config& c) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        bool hasValue = i + 1 < argc;
        if (a == "--port" && hasValue) {
            c.port = std::atoi(argv[++i]);
            if (c.port < 1 || c.port > 65535) {
                std::fprintf(stderr, "Port must be between 1 and 65535.\n");
                return false;
            }
        } else if (a == "--host" && hasValue) {
            c.host = argv[++i];
        } else if (a == "--data" && hasValue) {
            c.dataDir = argv[++i];
        } else if (a == "--create-admin") {
            c.createAdmin = true;
        } else if (a == "--seed-demo") {
            c.seedDemo = true;
        } else if (a == "--help" || a == "-h") {
            std::printf("Usage: munsphere [--port N] [--host IPv4] [--data folder] [--create-admin] [--seed-demo]\n");
            return false;
        } else {
            std::fprintf(stderr, "Unknown option '%s'. Try --help.\n", a.c_str());
            return false;
        }
    }
    return true;
}

// Reads a line without echoing it, so a password isn't shown on screen.
std::string readHidden(const char* prompt) {
    std::printf("%s", prompt);
    std::fflush(stdout);
    std::string s;
#ifdef _WIN32
    for (;;) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n') break;
        if (ch == 3) std::exit(1);                       // Ctrl+C
        if (ch == 8) { if (!s.empty()) s.pop_back(); continue; }  // Backspace
        if (ch == 0 || ch == 224) { _getch(); continue; }  // arrow/function keys
        s += static_cast<char>(ch);
    }
#else
    termios oldT{}, newT{};
    bool tty = tcgetattr(STDIN_FILENO, &oldT) == 0;
    if (tty) {
        newT = oldT;
        newT.c_lflag &= static_cast<tcflag_t>(~ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newT);
    }
    std::getline(std::cin, s);
    if (tty) tcsetattr(STDIN_FILENO, TCSANOW, &oldT);
#endif
    std::printf("\n");
    return s;
}

int createAdmin(Auth& auth) {
    std::printf("Create an Admin account\n");
    std::printf("-----------------------\n");
    std::string name;
    std::printf("Full name: ");
    std::fflush(stdout);
    std::getline(std::cin, name);
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::string pw = readHidden("Password (8+ characters, a letter and a digit): ");
        std::string again = readHidden("Repeat the password: ");
        if (pw != again) {
            std::printf("The passwords don't match. Try again.\n");
            continue;
        }
        try {
            User u = auth.createAccount(ROLE_ADMIN, name, pw, false);
            AuditManager(auth).record("console", "admin", "Created Admin account from the console", "new=" + u.id(), 201);
            std::printf("\nCreated Admin account %s for %s.\n", u.id().c_str(), u.name().c_str());
            std::printf("Start the server with run.bat and log in with ID %s.\n", u.id().c_str());
            return 0;
        } catch (const ApiError& e) {
            std::printf("%s\n", e.what());
            if (e.code() != "WEAK_PASSWORD") return 1;
        }
    }
    std::printf("No account was created.\n");
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    if (!parseArgs(argc, argv, cfg)) return 1;

    if (!cfg.createAdmin && !std::filesystem::exists(cfg.frontendDir + "/index.html")) {
        std::fprintf(stderr,
                     "Can't find %s/index.html. Run MUNSphere from the project root folder "
                     "(the one that contains run.bat).\n",
                     cfg.frontendDir.c_str());
        return 1;
    }
    try {
        Database::instance().open(cfg.dataDir);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Startup failed: %s\n", e.what());
        return 1;
    }
    if (Database::instance().skippedLines() > 0)
        std::fprintf(stderr, "Warning: %d malformed line(s) in the data files were skipped.\n",
                     Database::instance().skippedLines());

    Auth auth;
    if (cfg.createAdmin) return createAdmin(auth);

    // Managers live for the whole program; routes capture them by reference.
    ConferenceManager conferences;
    EBManager eb(auth, conferences);
    DelegateManager delegates(auth, conferences);
    CommitteeManager committees(delegates, conferences);
    AttendanceManager attendance(conferences);
    SpeakerManager speakers(conferences, attendance);
    VotingManager voting(conferences, attendance);
    MotionManager motions(conferences, attendance, voting, speakers);
    ScoringManager scoring(conferences, attendance);
    RankingManager rankings(conferences, attendance, scoring);
    ReportManager reports(conferences, attendance, scoring, rankings);
    AuditManager audit(auth);
    PersistenceManager persistence(audit);

    Router router;
    router.get("/api/health", [](const HttpRequest&) {
        Json j = Json::object();
        j.set("name", "MUNSphere").set("phase", "1.0: all 12 modules")
            .set("serverTime", nowTimestamp());
        return HttpResponse::ok(j);
    });
    registerAuthRoutes(router, auth);
    registerModule1Routes(router, conferences, auth);
    registerDelegateRoutes(router, delegates, auth);
    registerCommitteeRoutes(router, committees, auth);
    registerEBRoutes(router, eb, auth);
    registerAttendanceRoutes(router, attendance, conferences, auth);
    registerSpeakerRoutes(router, speakers, auth);
    registerMotionRoutes(router, motions, auth);
    registerVotingRoutes(router, voting, auth);
    registerLiveRoutes(router, auth, conferences, attendance, speakers, motions, voting);
    registerScoringRoutes(router, scoring, auth);
    registerRankingRoutes(router, rankings, auth);
    registerReportRoutes(router, reports, auth);
    registerAuditRoutes(router, audit, auth);
    registerPersistenceRoutes(router, persistence, auth);
    audit.attach(router);   // every change made through the API is recorded from here on
    if (cfg.seedDemo) return seedDemo(router, auth);

    HttpServer server(router);
    server.addStaticMount("/", cfg.frontendDir);
    server.addStaticMount("/module1/", cfg.module1Dir);
    server.addStaticMount("/module2/", cfg.module2Dir);
    if (!server.start(cfg.host, cfg.port)) return 1;

    std::printf("MUNSphere is running.\n");
    std::printf("  App:          http://%s:%d/\n", cfg.host.c_str(), cfg.port);
    std::printf("  Data folder:  %s\n", cfg.dataDir.c_str());
    std::printf("  API routes:   %lu\n", static_cast<unsigned long>(router.routeCount()));
    if (account_count_role(ROLE_ADMIN) == 0) {
        std::printf("\n  No Admin account exists yet. Stop the server (Ctrl+C) and run:\n");
        std::printf("      .\\run.bat admin\n");
    }
    std::printf("\nPress Ctrl+C to stop. Every change is saved as it happens.\n\n");
    std::fflush(stdout);
    server.run();
    return 0;
}
