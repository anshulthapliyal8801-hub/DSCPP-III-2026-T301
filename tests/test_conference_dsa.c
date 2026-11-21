/*
 * MUNSphere — C unit tests for Module 1 (conference_dsa.c)
 * Run from the project root: build\test_conference_dsa.exe
 */
#include <stdio.h>
#include <string.h>
#include "dsa_api.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { g_pass++; }                                           \
        else { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, msg); } \
    } while (0)

static ConferenceRec make_conf(const char *id, const char *name)
{
    ConferenceRec r;
    memset(&r, 0, sizeof r);
    strcpy(r.id, id);
    strcpy(r.name, name);
    strcpy(r.venue, "Graphic Era, Dehradun");
    strcpy(r.startDate, "2026-12-01");
    strcpy(r.endDate, "2026-12-03");
    return r;
}

static CommitteeRec make_com(const char *id, const char *conf,
                             const char *name, const char *abbr)
{
    CommitteeRec r;
    memset(&r, 0, sizeof r);
    strcpy(r.id, id);
    strcpy(r.conferenceId, conf);
    strcpy(r.name, name);
    strcpy(r.abbreviation, abbr);
    r.capacity = 40;
    return r;
}

static SessionRec make_ses(const char *id, const char *com, const char *date,
                           const char *start, const char *end)
{
    SessionRec r;
    memset(&r, 0, sizeof r);
    strcpy(r.id, id);
    strcpy(r.committeeId, com);
    strcpy(r.title, "Session");
    strcpy(r.date, date);
    strcpy(r.startTime, start);
    strcpy(r.endTime, end);
    r.status = SESSION_SCHEDULED;
    return r;
}

static void test_empty(void)
{
    ConferenceRec out;
    CommitteeRec arr[4];
    int cmp = -1;
    printf("Empty store\n");
    m1_store_reset();
    CHECK(conference_count() == 0, "count is 0");
    CHECK(conference_find("CONF001", &out, &cmp) == DSA_ERR_NOT_FOUND,
          "find on empty list is NOT_FOUND");
    CHECK(cmp == 0, "zero comparisons on empty list");
    CHECK(conference_delete("CONF001") == DSA_ERR_NOT_FOUND,
          "delete on empty list is NOT_FOUND");
    CHECK(committee_search_name("x", arr, 4, &cmp) == 0, "search finds nothing");
    CHECK(m1_max_id_suffix(M1_CONFERENCE, "CONF") == 0, "max suffix is 0");
}

static void test_single_and_duplicate(void)
{
    ConferenceRec a = make_conf("CONF001", "MUNSphere 2026"), out;
    int cmp;
    printf("Single record and duplicates\n");
    m1_store_reset();
    CHECK(conference_insert(&a) == DSA_OK, "insert ok");
    CHECK(conference_insert(&a) == DSA_ERR_DUPLICATE, "duplicate rejected");
    CHECK(conference_count() == 1, "count stays 1 after duplicate");
    CHECK(conference_find("CONF001", &out, &cmp) == DSA_OK, "find ok");
    CHECK(strcmp(out.name, "MUNSphere 2026") == 0, "found record has right name");
    CHECK(cmp == 1, "one comparison for head node");
    strcpy(a.name, "Renamed");
    CHECK(conference_update(&a) == DSA_OK, "update ok");
    conference_find("CONF001", &out, NULL);
    CHECK(strcmp(out.name, "Renamed") == 0, "update visible");
    a.id[0] = '\0';
    CHECK(conference_insert(&a) == DSA_ERR_INVALID, "empty ID rejected");
}

static void test_delete_positions(void)
{
    ConferenceRec a = make_conf("C1", "A"), b = make_conf("C2", "B"),
                  c = make_conf("C3", "C"), d = make_conf("C4", "D");
    ConferenceRec arr[8];
    int n, cmp;
    printf("Delete head / middle / tail\n");
    m1_store_reset();
    conference_insert(&a); conference_insert(&b);
    conference_insert(&c); conference_insert(&d);
    conference_find("C4", NULL, &cmp);
    CHECK(cmp == 4, "linear search reaches 4th node in 4 comparisons");

    CHECK(conference_delete("C2") == DSA_OK, "delete middle");
    CHECK(conference_delete("C1") == DSA_OK, "delete head");
    CHECK(conference_delete("C4") == DSA_OK, "delete tail");
    n = conference_to_array(arr, 8);
    CHECK(n == 1 && strcmp(arr[0].id, "C3") == 0, "only C3 remains");
    /* After deleting the tail, appending must still work (tail fixed) */
    CHECK(conference_insert(&a) == DSA_OK, "append after tail delete");
    n = conference_to_array(arr, 8);
    CHECK(n == 2 && strcmp(arr[1].id, "C1") == 0, "appended at end");
    CHECK(conference_delete("C3") == DSA_OK && conference_delete("C1") == DSA_OK,
          "delete remaining");
    CHECK(conference_count() == 0, "list empty again");
    CHECK(conference_insert(&b) == DSA_OK && conference_count() == 1,
          "insert into list emptied by deletes");
}

static void test_committee_search(void)
{
    CommitteeRec a = make_com("COM001", "CONF001", "General Assembly", "UNGA");
    CommitteeRec b = make_com("COM002", "CONF001", "All India Political Parties Meet", "AIPPM");
    CommitteeRec c = make_com("COM003", "CONF002", "Security Council", "UNSC");
    CommitteeRec out[8];
    int n, cmp;
    printf("Committee name search and counts\n");
    m1_store_reset();
    committee_insert(&a); committee_insert(&b); committee_insert(&c);
    n = committee_search_name("council", out, 8, &cmp);
    CHECK(n == 1 && strcmp(out[0].id, "COM003") == 0, "case-insensitive name match");
    CHECK(cmp == 3, "every node examined once");
    n = committee_search_name("ga", out, 8, &cmp);
    CHECK(n == 1 && strcmp(out[0].id, "COM001") == 0, "abbreviation match");
    n = committee_search_name("", out, 8, &cmp);
    CHECK(n == 3, "empty query matches all");
    n = committee_search_name("", out, 2, &cmp);
    CHECK(n == 2, "result capped at max");
    CHECK(committee_count_by_conference("CONF001") == 2, "two in CONF001");
    CHECK(m1_max_id_suffix(M1_COMMITTEE, "COM") == 3, "max suffix 3");
}

static void test_session_sorting(void)
{
    SessionRec s1 = make_ses("SES001", "COM001", "2026-12-02", "10:00", "11:00");
    SessionRec s2 = make_ses("SES002", "COM001", "2026-12-01", "14:00", "15:00");
    SessionRec s3 = make_ses("SES003", "COM001", "2026-12-01", "09:00", "10:00");
    SessionRec arr[8];
    int n;
    printf("Session sorted insertion and re-sorting update\n");
    m1_store_reset();
    session_insert(&s1); session_insert(&s2); session_insert(&s3);
    n = session_to_array(arr, 8);
    CHECK(n == 3, "three sessions");
    CHECK(strcmp(arr[0].id, "SES003") == 0 && strcmp(arr[1].id, "SES002") == 0 &&
          strcmp(arr[2].id, "SES001") == 0, "ordered by date then time");

    strcpy(s3.date, "2026-12-03");                 /* move earliest to last */
    CHECK(session_update(&s3) == DSA_OK, "update ok");
    n = session_to_array(arr, 8);
    CHECK(strcmp(arr[0].id, "SES002") == 0 && strcmp(arr[2].id, "SES003") == 0,
          "update re-positions node");
    CHECK(session_count() == 3, "count unchanged by update");

    s1.status = 7;
    CHECK(session_update(&s1) == DSA_ERR_INVALID, "invalid status rejected");
    CHECK(strcmp(session_status_name(SESSION_ACTIVE), "Active") == 0, "status name");
    CHECK(strcmp(session_status_name(99), "Unknown") == 0, "bad status name");
    CHECK(session_count_by_committee("COM001") == 3, "count by committee");
    CHECK(session_delete("SES002") == DSA_OK, "delete head session");
    n = session_to_array(arr, 8);
    CHECK(n == 2 && strcmp(arr[0].id, "SES001") == 0, "new head is SES001");
}

static void test_persistence(void)
{
    const char *p1 = "tests/tmp_conferences.txt";
    const char *p2 = "tests/tmp_committees.txt";
    const char *p3 = "tests/tmp_sessions.txt";
    ConferenceRec a = make_conf("CONF001", "Name | with pipe \\ and slash");
    CommitteeRec c = make_com("COM001", "CONF001", "UNGA", "UNGA");
    SessionRec s = make_ses("SES001", "COM001", "2026-12-01", "09:00", "10:00");
    ConferenceRec out;
    CommitteeRec cout;
    int skipped = -1;
    FILE *f;

    printf("Persistence round-trip, missing and malformed files\n");
    m1_store_reset();
    strcpy(a.description, "Line one\nLine two");
    conference_insert(&a);
    committee_insert(&c);
    s.status = SESSION_ACTIVE;
    session_insert(&s);
    CHECK(m1_save(p1, p2, p3) == DSA_OK, "save ok");

    m1_store_reset();
    CHECK(m1_load(p1, p2, p3, &skipped) == DSA_OK, "load ok");
    CHECK(skipped == 0, "nothing skipped");
    CHECK(conference_find("CONF001", &out, NULL) == DSA_OK, "conference reloaded");
    CHECK(strcmp(out.name, "Name | with pipe \\ and slash") == 0,
          "pipe and backslash survive escaping");
    CHECK(strcmp(out.description, "Line one\nLine two") == 0, "newline survives");
    CHECK(committee_find("COM001", &cout, NULL) == DSA_OK && cout.capacity == 40,
          "committee capacity reloaded");
    {
        SessionRec sout;
        CHECK(session_find("SES001", &sout, NULL) == DSA_OK &&
              sout.status == SESSION_ACTIVE, "session status reloaded");
    }

    /* Missing files: empty store, not an error */
    CHECK(m1_load("tests/none1.txt", "tests/none2.txt", "tests/none3.txt",
                  &skipped) == DSA_OK, "missing files load as empty");
    CHECK(conference_count() == 0, "store empty after loading missing files");

    /* Reload real data, then try a corrupt header: store must be untouched */
    m1_load(p1, p2, p3, &skipped);
    f = fopen("tests/tmp_bad.txt", "wb");
    fputs("this is not a MUNSphere file\n", f);
    fclose(f);
    CHECK(m1_load("tests/tmp_bad.txt", p2, p3, &skipped) == DSA_ERR_INVALID,
          "bad header rejected");
    CHECK(conference_count() == 1, "store unchanged after rejected load");

    /* Malformed lines are skipped and counted, valid ones still load */
    f = fopen("tests/tmp_bad.txt", "wb");
    fputs("#MUNSPHERE|conferences|1\n", f);
    fputs("CONF009|Good|Venue|2026-01-01|2026-01-02|ok\n", f);
    fputs("too|few|fields\n", f);
    fputs("CONF009|Duplicate|V|2026-01-01|2026-01-02|x\n", f);
    fputs("BAD\\qESCAPE|a|b|c|d|e\n", f);
    fclose(f);
    CHECK(m1_load("tests/tmp_bad.txt", "tests/none2.txt", "tests/none3.txt",
                  &skipped) == DSA_OK, "file with bad lines still loads");
    CHECK(skipped == 3, "three bad lines counted");
    CHECK(conference_count() == 1, "only the valid line loaded");

    remove(p1); remove(p2); remove(p3); remove("tests/tmp_bad.txt");
}

static void test_large_dataset(void)
{
    char id[DSA_ID_LEN];
    int i, cmp, ok = 1;
    ConferenceRec r;
    printf("Large dataset (5000 records)\n");
    m1_store_reset();
    for (i = 1; i <= 5000; i++) {
        snprintf(id, sizeof id, "CONF%04d", i);
        r = make_conf(id, "Bulk");
        if (conference_insert(&r) != DSA_OK) ok = 0;
    }
    CHECK(ok && conference_count() == 5000, "5000 inserts");
    conference_find("CONF5000", NULL, &cmp);
    CHECK(cmp == 5000, "worst case linear search = n comparisons");
    conference_find("CONF0001", NULL, &cmp);
    CHECK(cmp == 1, "best case linear search = 1 comparison");
    CHECK(m1_max_id_suffix(M1_CONFERENCE, "CONF") == 5000, "max suffix 5000");
    m1_store_reset();
    CHECK(conference_count() == 0, "reset frees all");
}

int main(void)
{
    printf("=== Module 1 C data-structure tests ===\n");
    test_empty();
    test_single_and_duplicate();
    test_delete_positions();
    test_committee_search();
    test_session_sorting();
    test_persistence();
    test_large_dataset();
    printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
