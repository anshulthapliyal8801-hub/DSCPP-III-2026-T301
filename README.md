# MUNSphere

**Model United Nations Management System Using Data Structures in C and
Object-Oriented Programming in C++**

MUNSphere runs a whole MUN conference: delegate registration and placement,
Executive Board accounts, live committee sessions (attendance, speakers list,
points of information, motions, voting), marksheets with an approval workflow,
rankings and awards, reports, backups and an audit log.

One C++ server keeps every record in C data structures, saves each change to
text files the moment it happens, and serves a browser application to three
kinds of user: **Delegates**, the **Executive Board** and the **Admin**. Nothing
needs to be installed except MinGW-w64.

| | |
|---|---|
| Modules | All 12 implemented |
| API endpoints | 130, each classified and tested for every role |
| Automated checks | 2,269 in 11 suites (C and C++), plus browser tests |
| External libraries | None |

---

## 1. Quick start

Requirements: Windows 10/11, **MinGW-w64 GCC/G++** (developed against 14.2.0)
with its `bin` folder on `PATH`, and a modern browser. Check with `gcc --version`.

Open PowerShell in the `MUNSphere` folder:

```powershell
.\run.bat demo      # fastest way to see everything: builds, fills demo-data\, starts
```

Then open **http://127.0.0.1:8080/**. The console prints the logins:

| Who | ID | Password |
|---|---|---|
| Admin | `ADM001` | `Admin2026` |
| UNGA Chairperson (Moderator `EB002`, Scribe `EB003`) | `EB001` | `Board2026` |
| UNSC Chairperson / AIPPM Chairperson | `EB004` / `EB006` | `Board2026` |
| Delegates (`D001` India in UNGA, Best Delegate) | `D001` to `D024` | `Delegate2026` |

The demo data lives in `demo-data\`, separate from real data. Delete that
folder to start the demo afresh. [docs/DEMO.md](docs/DEMO.md) is a step-by-step
demonstration script.

### For a real conference

```powershell
.\run.bat admin     # once: create the Admin account (password hidden as you type)
.\run.bat           # build and start; data is kept in data\
```

| Command | What it does |
|---|---|
| `.\run.bat` | Build, then start the server |
| `.\run.bat run --port 9090` | Start with options (`--port`, `--host`, `--data`) |
| `.\run.bat admin` | Create an Admin account from the console |
| `.\run.bat demo` | Build, seed `demo-data\` if empty, start on it |
| `.\run.bat test` | Build and run every C and C++ test suite |
| `.\run.bat build` / `clean` | Build only / delete `build\` |

The server listens on `127.0.0.1` (this computer). To let others on a trusted
private network connect: `.\run.bat run --host 0.0.0.0`. Traffic is plain HTTP,
so never expose it to the open internet. Stop with **Ctrl+C**; nothing is lost.

---

## 2. What each role can do

**Delegates** register themselves and get an ID (`D001`…). They apply to a
conference with up to three ranked committee preferences, see the decision (and
the reason if rejected), and once placed see their own placard. In a live session
they check in, request to speak, see their place in the speakers list and the
speaking clock, raise POIs, propose motions and vote. After publication they see
their marksheet, rank and award. They never see another delegate's marks.

**Executive Board** members are created by the Admin (`EB001`…, temporary
password, forced change at first login) and work only in their own committee,
within their role's permissions: *Chairperson* (everything), *Moderator*
(the floor: speakers, POIs, motions, voting, attendance), *Scribe* (attendance
and reports). They run the live floor from a chair console, mark attendance,
write marksheets, see a provisional ranking and run committee reports.

**The Admin** sets up conferences, committees and sessions; approves and places
delegates (capacity and one-portfolio-per-committee enforced); manages EB
accounts, permissions, transfers and revocation; approves, returns and publishes
marksheets (but can't write marks); configures scoring criteria, awards and the
tie-break; and has the live monitor, ten reports with CSV/text export, DSA
analytics, announcements, backups, the consistency check and the audit log.

---

## 3. Project structure

```
MUNSphere/
├── Module1_Conference/    conference_dsa.c, Conference.cpp, server.cpp, main.cpp, DemoSeed.cpp, frontend/ (Module 1 lab)
├── Module2_Delegate/      delegate_dsa.c (hash table + BST), Delegate.cpp, DelegateManager.cpp, frontend/ (Module 2 lab)
├── Module3_Committee/     committee_dsa.c, Committee.cpp, CommitteeManager.cpp
├── Module4_Attendance/    attendance_dsa.c, Attendance.cpp, AttendanceManager.cpp
├── Module5_SpeakerList/   speaker_dsa.c (circular queue), Speaker.cpp, SpeakerManager.cpp, LiveSession.cpp
├── Module6_Motion/        motion_dsa.c (heap + stack), Motion.cpp, MotionManager.cpp
├── Module7_Voting/        voting_dsa.c, Vote.cpp, VotingManager.cpp
├── Module8_Scoring/       scoring_dsa.c, Score.cpp, ScoringManager.cpp
├── Module9_Awards/        awards_dsa.c (five sorts), Award.cpp, RankingManager.cpp
├── Module10_ExecutiveBoard/ eb_dsa.c (adjacency list), Person.cpp, EBMember.cpp, EBManager.cpp
├── Module11_Reports/      reports_dsa.c (statistics, CSV), Report.cpp, ReportManager.cpp
├── Module12_Persistence/  persistence_dsa.c (file layer, audit log), PersistenceManager.cpp, AuditManager.cpp
├── Shared/                dsa_api.h, account_dsa.c, Auth.cpp, User.cpp, RoleManager.cpp, API.cpp, Database.cpp, Common.cpp
├── frontend/              index, login, admin, eb, delegate pages; css/; js/; assets/
├── tests/                 11 test suites
├── docs/                  ARCHITECTURE.md, MODULES.md, DEMO.md
├── data/                  created on first run; one text file per store, plus backups/
├── run.bat, CMakeLists.txt, README.md
```

Every class has a matching `.h`. Files beyond the required layout, and why:
`Shared/dsa_api.h` (single C/C++ contract), `Shared/account_dsa.c` (accounts
are shared by all roles), `server.h`, `LiveSession.cpp` (composes Modules 4–7),
`DemoSeed.cpp` (drives every module).

---

## 4. Building by hand and with CMake

`run.bat` compiles each `.c` with `gcc -std=c11` and each `.cpp` with
`g++ -std=c++17` to an object once, then links with
`-lws2_32 -lbcrypt -static` (Winsock; the Windows random number generator for
salts, tokens and temporary passwords; a self-contained exe). It discovers new
files automatically.

Optional CMake build:

```powershell
cmake -S . -B build-cmake -G "MinGW Makefiles"
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

---

## 5. Tests

`.\run.bat test` builds and runs all eleven suites. They use temporary folders in
`tests\` and clean up after themselves.

| Suite | Checks | Covers |
|---|---|---|
| `test_conference_dsa.c` | 63 | Module 1 linked lists, sorted insertion, file round trips, corrupt files |
| `test_people_dsa.c` | 82 | Account hash table, login history, EB adjacency list, delegate hash table (rehash at 0.75, collisions) and BST (all deletion cases, traversals), searches, roster sort, 5,000 records |
| `test_live_dsa.c` | 79 | Attendance arrays, circular queue wrap-around and reload, request and POI queues, motion heap under 300 random operations, undo stack, ballots |
| `test_scoring_dsa.c` | 61 | Criteria and marksheet arrays, revisions, all five sorts against `qsort` on every input shape, exact textbook comparison counts |
| `test_reports_dsa.c` | 31 | Audit log including a torn last line, statistics against hand-computed values, histogram, CSV escaping, announcements |
| `test_module1.cpp` | 71 | JSON parser, validation, router, every Module 1 rule through the API |
| `test_auth_people.cpp` | 102 | Login, lockout, PBKDF2, password workflows, every authorization rule in the specification, registration and placement |
| `test_live.cpp` | 83 | A full live session: attendance, speakers, POIs, motions, votes, role rules, persistence |
| `test_scoring.cpp` | 60 | Marksheet workflow and separation of duties, rankings with ties, awards, benchmarks, snapshots |
| `test_reports.cpp` | 53 | Audit hook and hash chain, reports and exports, analytics, announcements, backup/restore, round trip, integrity |
| `test_authz.cpp` | 1,584 | **Authorization matrix**: all 130 routes × 6 identities on demo data; fails if any route is unclassified; no 500 on odd input |

All suites also pass under AddressSanitizer and UndefinedBehaviorSanitizer with
`-Wall -Wextra -Wpedantic -Wshadow` and no warnings. The browser application was
tested end to end in Chromium (every page for every role at desktop and phone
widths, and full workflows), with no console errors.

---

## 6. API overview

All responses are JSON: `{"ok":true,"data":...}` or
`{"ok":false,"error":{"code":"...","message":"..."}}`. Every endpoint except
login, logout, delegate registration, password-reset requests and
`/api/public/live` needs `Authorization: Bearer <token>`.

| Area | Endpoints |
|---|---|
| Auth | `POST /api/auth/login`, `/logout`, `/register`, `/change-password`, `/request-reset`; `GET /api/auth/me`, `/history` |
| Accounts | `GET /api/admin/accounts`, `/login-history`; `POST /api/admin/accounts/:id/activate\|deactivate\|reset-password` |
| Conference setup | `/api/conferences`, `/api/committees`, `/api/sessions` (CRUD); `POST /api/sessions/:id/activate\|deactivate\|complete`; `GET /api/overview` |
| Delegates | `/api/delegate/me` (+ `/registration`, `/sessions`, `/attendance`, `/pois`, `/marksheet`, `/results`); `/api/delegates` (+ `/:id/approve\|reject\|assign\|unassign`) |
| Committees, EB | `GET /api/committees/:id/roster`, `/api/committee-summaries`; `/api/eb` (+ `/:id`, `/revoke`, `/restore`, `/me`, `/permissions`) |
| Live floor | `GET /api/sessions/:id/live`, `/api/live/current`, `/api/live/overview`; attendance, check-in, speakers, speak-requests, POIs, motions, votes under `/api/sessions/:id/…`, `/api/pois/:id/…`, `/api/motions/:id/…`, `/api/votes/:id/…` |
| Results | `/api/scoring/criteria`, `/api/committees/:id/marksheets`, `/api/marksheets` (+ `/submit\|approve\|return\|publish`), `/api/awards/config`, `/api/committees/:id/rankings`, `/api/rankings/overall\|compare\|benchmark` |
| Insight | `GET /api/reports`, `/api/reports/:key?format=json\|csv\|txt`, `/api/analytics/dsa`; `/api/announcements` |
| Data | `/api/persistence/status\|verify`, `/api/backups` (+ `/:id/restore`), `/api/audit` (+ `/verify`, `/export`) |
| Labs | `/api/dsa/module1/…`, `/api/dsa/module2/…`; pages at `/module1/` and `/module2/` |

`tests/test_authz.cpp` contains the authoritative table of who may call each one.

---

## 7. Troubleshooting

| Message or problem | Fix |
|---|---|
| `gcc was not found` | Add MinGW's `bin` folder to `PATH`; open a new PowerShell |
| `run.bat is not recognized` | In PowerShell type `.\run.bat` |
| `Can't find frontend/index.html` | Run from the `MUNSphere` folder, not from `build\` |
| `Port 8080 is busy` | Close the other program, or `.\run.bat run --port 9090` |
| `No Admin account exists yet` | Stop the server and run `.\run.bat admin` |
| `... is not a valid MUNSphere file. It was left untouched.` | A data file was damaged or edited. Restore a backup (Admin → Data & audit), or copy the files from `data\backups\<id>\` into `data\` by hand |
| `Too many failed attempts` | Wait 5 minutes, or ask an Admin to reset the password |
| Forgot the only Admin password | Stop the server, `.\run.bat admin` to create a second Admin, log in, reset the first |
| Everyone was logged out | The server was restarted (logins live in memory); log in again |
| Windows Firewall prompt | *Private networks only*, or cancel if only this computer uses it |

---

## 8. Documentation

* [docs/DEMO.md](docs/DEMO.md): the end-to-end demonstration, step by step
* [docs/MODULES.md](docs/MODULES.md): all 12 modules: data structures, OOP, rules, tests
* [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): diagrams, design decisions, security, viva questions
