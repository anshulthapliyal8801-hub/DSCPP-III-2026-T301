# MUNSphere: the 12 modules

For each module: what it does, the C data structures (file, operations,
complexity), the C++ classes and OOP concepts, the rules it enforces, and where
it is tested. Complexities use n = number of records in that structure.

The C files hold the data; the C++ classes enforce the rules and expose the API.
They meet only through `Shared/dsa_api.h` (see ARCHITECTURE.md §3).

---

## Module 1: Conference and session management

**Purpose.** Conferences, committees and sessions, and the session lifecycle.

| C (`conference_dsa.c`) | Detail | Cost |
|---|---|---|
| Three singly linked lists | Conferences and committees with head and tail (append O(1)); sessions with head only | |
| Sorted insertion | Sessions kept in (date, start time, ID) order on every insert and update | O(n) |
| Linear search | Find by ID, with comparison counting; committee name search | O(n) |
| Deletion | Search keeps the previous node; tail pointer fixed when the tail goes | O(n) |
| Arrays | `*_to_array` export; status-name lookup table | O(n) |
| File handling | Through the Module 12 helpers | O(n) |

**C++ (`Conference.cpp`, `Committee.cpp`).** `Conference`, `Committee`, `Session`
derive from the abstract `Entity` (`toJson`, `entityName` pure virtual).
Setters validate, so an invalid object can't exist (encapsulation). `Session` is
a state machine: scheduled → active → completed, active ↔ scheduled.

**Rules.** Valid dates within the conference; no overlapping sessions in a
committee; one active session per committee; active or completed sessions can't
be edited or deleted; a conference with committees, or a committee with
sessions, delegates or EB members, can't be deleted (other modules register
these checks as callbacks); capacity can't drop below the delegates placed.

**Tests.** `test_conference_dsa.c` (63), `test_module1.cpp` (71).

## Module 2: Delegate registration and management

**Purpose.** Self-registration with a unique Delegate ID, profiles, conference
registration with preferences, approval, duplicate prevention.

| C (`delegate_dsa.c`) | Detail | Cost |
|---|---|---|
| Hash table (primary store) | djb2 hash, separate chaining, starts at 8 buckets | find O(1) average |
| Rehashing | Doubles when the load factor would pass 0.75; nodes relinked, not copied | O(n), amortized O(1) per insert |
| BST (secondary index) | Keyed by (name, ID); stores keys only, so each record exists once | O(h) |
| BST deletion | Leaf, one child, two children (in-order successor) | O(h) |
| BST queries | In-, pre-, post-order and level order; min, max, height, node count | O(n) / O(h) |
| Linear search | By ID over an unsorted array copy | O(n) |
| Binary search | By ID over an array sorted with insertion sort first | O(log n) search |
| Bucket inspection | Bucket lengths and contents, load factor, resize count | O(n) |

**C++.** `Delegate` inherits `Person` (which *has* a `User` account:
composition) and adds a registration state machine:
none → pending → approved, pending → rejected (with reason) → pending.
`DelegateManager` handles registration, profiles, approval and the lab.

**Rules.** One account per email address; 1–3 distinct preferences, all in the
chosen conference; an approved registration can only be changed by the Admin.

**Tests.** `test_people_dsa.c` (hash and BST sections), `test_auth_people.cpp`.

## Module 3: Committee management

**Purpose.** Placing delegates: committee, portfolio, capacity, rosters.

| C (`committee_dsa.c`) | Detail | Cost |
|---|---|---|
| Insertion sort | Rosters by name, portfolio or ID; stable; counts comparisons and moves | O(n²), O(n) nearly sorted |
| Linear search | Case-insensitive portfolio clash check | O(n) |

Rosters are built from the delegate store when needed, so no delegate data is
copied.

**C++.** `Committee` (shared with Module 1), `CommitteeManager`.

**Rules.** Only approved delegates are placed; the committee must belong to their
conference; capacity; one delegate per portfolio per committee.

**Tests.** `test_people_dsa.c` (roster), `test_auth_people.cpp` (placement).

## Module 4: Attendance

**Purpose.** Present, late, absent and delegate check-in, percentages, reports.

| C (`attendance_dsa.c`) | Detail | Cost |
|---|---|---|
| Linked list of session sheets | One sheet per session that has attendance | |
| Dynamic array per sheet | Doubles when full (`realloc`) | append O(1) amortized |
| Duplicate prevention | Marking again updates the existing entry | O(n) search |
| Tally | Counts per status into an array | O(n) |

**C++.** `AttendanceEntry`, `AttendanceManager`. `requireSeated()` is the gate
every delegate action in Modules 5–7 passes through.

**Rules.** Attendance opens when a session is activated and can be corrected
afterwards; *late* counts as attending; only *present* or *late* delegates may
speak, raise POIs, move motions or vote; checking in alone is not enough.

**Tests.** `test_live_dsa.c`, `test_live.cpp`.

## Module 5: Live speaker list and points of information

| C (`speaker_dsa.c`) | Detail | Cost |
|---|---|---|
| Circular queue (GSL) | 64 slots, `head` and `count`, indices modulo 64 | enqueue/dequeue O(1); remove from middle O(n) |
| Linked-list queue | Speaking requests, head and tail | push O(1) |
| Speech history | Linked list, newest first; actual and allotted seconds | push O(1) |
| POIs | List of all POIs, plus a FIFO queue of pointers to pending ones | O(1) enqueue |

**C++.** `PointOfInformation` (state machine: pending → accepted, rejected with a
reason, or withdrawn by its author), `SpeakerManager`, and `LiveSession.cpp`,
which assembles one live snapshot from Modules 4–7, shaped by role.

**Rules.** No duplicates in the list or among requests; 10–600 seconds per
speech; POIs go to the current speaker and can't be raised to yourself; only the
author withdraws a POI (checked before its state, so strangers learn nothing).
The speaking clock is computed from the server's time.

**Tests.** `test_live_dsa.c` (including wrap-around and reload), `test_live.cpp`.

## Module 6: Motion management

| C (`motion_dsa.c`) | Detail | Cost |
|---|---|---|
| Binary min-heap per session | Key: precedence, longer duration first, arrival order | insert, take top, remove by ID O(log n) |
| Stack per session | Chair actions for undo | push, pop O(1) |
| Linked list | All motions, the permanent record | |

**C++.** `Motion` with a precedence table (adjourn, suspend, close debate,
unmoderated caucus, moderated caucus, introduce draft, set speaking time) and the
fields and majority each needs; `MotionManager`.

**Rules.** The chair always takes the highest-precedence motion; taking it opens a
procedural vote with the motion's majority (two-thirds to close debate); the
vote's result marks the motion passed or failed, and a passed speaking-time
motion changes the speakers list timer; only the proposer withdraws; undo only
while the action still holds (not after ballots or a result).

**Tests.** `test_live_dsa.c` (heap under 300 random operations), `test_live.cpp`.

## Module 7: Voting management

| C (`voting_dsa.c`) | Detail | Cost |
|---|---|---|
| Linked list of votes | Newest first | |
| Dynamic array of ballots per vote | Doubles when full | |
| Duplicate prevention | Linear search for the delegate before casting | O(n) |
| Correction | Changes a ballot and counts the correction | O(n) |
| Tally | One pass into `counts[3]` (yes, no, abstain) | O(n) |

**C++.** `Vote` holds the decision rule: simple majority passes if YES > NO;
two-thirds passes if 3·YES ≥ 2·(YES + NO). Abstentions only on substantive votes.

**Rules.** One open vote per session; only seated delegates of the committee vote;
one ballot each; the EB records roll-call votes and authorized corrections;
delegates see progress but not the running tally until the vote closes.

**Tests.** `test_live_dsa.c`, `test_live.cpp`.

## Module 8: Delegate scoring and marksheets

| C (`scoring_dsa.c`) | Detail | Cost |
|---|---|---|
| Criteria array | At most 10; insertion-sorted by display order | |
| Marksheet dynamic array | Parallel arrays inside each sheet snapshot the criteria | find O(n) |
| Total | One pass, each mark checked against its maximum | O(k) |
| Revision list | Head and tail; every action with all marks | append O(1) |

**C++.** `Marksheet` state machine: draft → submitted → approved → published,
with *returned* (and a note) from submitted or approved. `ScoringManager`.

**Rules.** Default criteria from the specification (Research 20, Speaking 20,
Participation 20, Diplomacy 15, POI and Motion Quality 15, Attendance 10); the
attendance mark starts from the real attendance rate; **separation of duties**:
only the committee's EB with *evaluate* writes marks, only the Admin approves,
returns or publishes; delegates see only their own sheet, only once published.

**Tests.** `test_scoring_dsa.c`, `test_scoring.cpp`.

## Module 9: Awards and ranking

| C (`awards_dsa.c`) | Comparisons | Moves | Stable |
|---|---|---|---|
| Bubble sort (early exit) | O(n²); n−1 if sorted | O(n²) swaps | Yes |
| Selection sort | always n(n−1)/2 | ≤ n−1 swaps | No |
| Insertion sort | O(n²); n−1 if sorted | O(n²) moves | Yes |
| Merge sort | ≤ n log₂ n | 2·n·log₂n moves | Yes |
| Quick sort (last-element pivot) | O(n log n) average; n(n−1)/2 sorted | swaps | No |

Quick sort recurses into the smaller part and loops on the larger, so its stack
stays O(log n) even in the worst case. The comparison is a total order (score,
tie-break criterion, attendance, Delegate ID), so all five algorithms give the
identical ranking; the tests check this against `qsort`.

**C++.** `Ranking` sorts with the chosen algorithm, measures real counts and time,
assigns competition ranks (1, 2, 2, 4) and awards; `RankingManager` adds
configuration, the five-way comparison and a benchmark on generated data.

**Rules.** Award rules (name, places, minimum score) applied in order down the
ranking; genuine ties share the rank and the award; delegates are ranked only
among published results.

**Tests.** `test_scoring_dsa.c`, `test_scoring.cpp`.

## Module 10: Executive Board management

| C (`eb_dsa.c`) | Detail | Cost |
|---|---|---|
| Adjacency list | Committees as vertices, each with a list of its EB members | members of one committee O(m) |
| Transfer | Unlink the node from one list, link into another; empty vertices removed | O(n) find, O(1) move |
| Load | Inserted in reverse file order so save → load → save is identical | O(n) |

**C++.** The hierarchy required by the specification:

```
Person (abstract) ── has a User (login account)
├── Delegate
└── EBMember (abstract)
    ├── Chairperson   all permissions
    ├── Moderator     floor: run sessions, speakers, POIs, motions, voting, attendance
    └── Scribe        attendance, reports
```

`EBMember::create()` is a factory; `defaultPermissions()`, `roleTitle()` and
`responsibilities()` are overridden; `EBMember::toJson()` extends
`Person::toJson()`. Lists hold `unique_ptr<EBMember>` and call through the base
class (runtime polymorphism).

**Rules.** EB accounts only by the Admin, with a temporary password changed at
first login; access only to the assigned committee, within the permissions;
role change resets permissions to the new role's defaults; revocation takes
effect on the very next request.

**Tests.** `test_people_dsa.c`, `test_auth_people.cpp`, `test_authz.cpp`.

## Module 11: Reports and analytics

| C (`reports_dsa.c`) | Detail |
|---|---|
| `stats_summary` | Count, min, max, sum in one pass; integer mean and population standard deviation; median from an insertion-sorted copy; integer square root by Newton's method |
| `stats_histogram` | Fixed-width buckets in an array |
| `csv_escape` | RFC 4180 quoting |
| Announcements | Linked list, newest first |

**C++.** `Report` is abstract; ten subclasses (conference, delegates, committee,
attendance, speakers, motions, voting, scoring, awards, data structure
performance) build tables, and the base class renders JSON, CSV and aligned text.
`ReportManager` also serves the DSA Analytics page and announcements.

**Rules.** EB members run the five committee reports for their own committee only;
announcements reach everyone, all delegates, the EB, or one committee.

**Tests.** `test_reports_dsa.c`, `test_reports.cpp`.

## Module 12: Persistence and audit

| C (`persistence_dsa.c`) | Detail |
|---|---|
| File format | Header line `#MUNSPHERE\|kind\|1`; `\|`-separated fields; escaping for `\`, `\|`, newline |
| Atomic save | Write `file.tmp`, flush, rename over `file` (`MoveFileExA` on Windows) |
| Safe load | Missing file = empty store; bad header = refuse and leave the file untouched; bad lines skipped and counted |
| File copy | Byte copy through a temporary file (backups) |
| Audit log | Dynamic array in memory; append-only file; a torn last line is skipped |

**C++.** `AuditManager` (one router hook records every change; SHA-256 hash
chain; verification), `PersistenceManager` (backup with SHA-256 manifest,
verified restore with automatic safety backup and rollback, save → load → save
round trip, cross-store integrity scan), and `Database`, the single gateway that
loads and saves every store.

**Rules.** Request contents (passwords, ballot choices) are never logged;
restoring never rolls back the audit log; a backup that doesn't match its
manifest is refused.

**Tests.** `test_reports_dsa.c`, `test_reports.cpp`, every suite's persistence
checks.

---

## Shared layer

| File | Role |
|---|---|
| `dsa_api.h` | Every C struct and function, declared once inside `extern "C"` |
| `account_dsa.c` | Accounts: hash table (101 buckets, chaining); login history: capped linked list |
| `Auth.cpp` | SHA-256, HMAC and PBKDF2 written from scratch; sessions; lockout; password workflows |
| `User.cpp`, `RoleManager.cpp` | The account entity; roles, EB permission bits, authorization checks |
| `API.cpp`, `Common.cpp` | Router, HTTP types, JSON parser/serializer, validation |
| `Database.cpp` | Singleton gateway to the one shared data store |
