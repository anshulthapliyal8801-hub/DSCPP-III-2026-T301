/*
 * MUNSphere — C tests for Module 11 (statistics, CSV, announcements) and the
 * Module 12 audit log.
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

static void test_audit(void)
{
    AuditRec a, out;
    int i, skipped = 0;
    FILE *f;
    printf("Audit log: append-only file\n");
    audit_store_reset();
    remove("tests/tmp_audit.txt");
    CHECK(audit_next_seq() == 1, "first sequence number is 1");
    memset(&a, 0, sizeof a);
    strcpy(a.actor, "ADM001"); strcpy(a.role, "admin"); strcpy(a.at, "2026-11-08 10:00:00");
    for (i = 0; i < 100; i++) {                    /* grows the array */
        a.seq = audit_next_seq();
        snprintf(a.action, sizeof a.action, "Action %d | with pipe", i);
        strcpy(a.target, "id=D001");
        a.status = 200;
        strcpy(a.hash, "abc");
        if (audit_append(&a, "tests/tmp_audit.txt") != DSA_OK) break;
    }
    CHECK(i == 100 && audit_count() == 100 && audit_next_seq() == 101, "100 entries appended");
    a.action[0] = '\0';
    CHECK(audit_append(&a, NULL) == DSA_ERR_INVALID, "empty action refused");
    /* Simulate a crash in the middle of an append: a torn last line */
    f = fopen("tests/tmp_audit.txt", "ab");
    fputs("101|2026-11-08 10:00:01|ADM0", f);
    fclose(f);
    audit_store_reset();
    CHECK(audit_load("tests/tmp_audit.txt", &skipped) == DSA_OK && audit_count() == 100 && skipped == 1,
          "torn last line skipped and counted, earlier entries intact");
    CHECK(audit_get(99, &out) == DSA_OK && out.seq == 100 && !strcmp(out.action, "Action 99 | with pipe"),
          "fields round-trip");
    CHECK(audit_get(100, &out) == DSA_ERR_NOT_FOUND, "out of range");
    CHECK(audit_save("tests/tmp_audit2.txt") == DSA_OK, "full rewrite for backups");
    audit_store_reset();
    CHECK(audit_load("tests/tmp_audit2.txt", &skipped) == DSA_OK && audit_count() == 100, "rewritten file loads");
    audit_store_reset();
    remove("tests/tmp_audit.txt");
    remove("tests/tmp_audit2.txt");
}

static void test_stats(void)
{
    StatSummary s;
    int v[] = {20, 40, 40, 40, 50, 50, 70, 90};
    int one[] = {73};
    int even[] = {10, 20, 30, 41};
    int pct[] = {0, 95, 100, 449, 450, 1000, 999, 1200, -5};
    int b[10];
    printf("Statistics and histogram\n");
    CHECK(stats_summary(v, 0, &s) == DSA_OK && s.count == 0, "empty input");
    stats_summary(v, 8, &s);
    CHECK(s.count == 8 && s.min == 20 && s.max == 90 && s.sum == 400, "count, min, max, sum");
    CHECK(s.meanTenths == 500, "mean 50.0");
    CHECK(s.stddevTenths == 200, "population standard deviation 20.0");
    CHECK(s.medianTenths == 450, "median of an even count: 45.0");
    stats_summary(one, 1, &s);
    CHECK(s.meanTenths == 730 && s.medianTenths == 730 && s.stddevTenths == 0, "single value");
    stats_summary(even, 4, &s);
    CHECK(s.medianTenths == 250 && s.meanTenths == 253, "median 25.0, mean 25.3 rounded");
    CHECK(v[0] == 20 && v[7] == 90, "input array not modified");
    stats_histogram(pct, 9, 0, 100, b, 10);
    CHECK(b[0] == 3 && b[4] == 2 && b[9] == 3, "values below, at and above the edges land in the right buckets");
    CHECK(stats_histogram(pct, 9, 0, 0, b, 10) == DSA_ERR_INVALID, "zero width refused");
}

static void test_csv(void)
{
    char out[64];
    printf("CSV escaping (RFC 4180)\n");
    CHECK(csv_escape("India", out, sizeof out) == 5 && !strcmp(out, "India"), "plain text unchanged");
    csv_escape("Sharma, Rahul", out, sizeof out);
    CHECK(!strcmp(out, "\"Sharma, Rahul\""), "comma: quoted");
    csv_escape("He said \"yes\"", out, sizeof out);
    CHECK(!strcmp(out, "\"He said \"\"yes\"\"\""), "quotes doubled inside quotes");
    csv_escape("two\nlines", out, sizeof out);
    CHECK(!strcmp(out, "\"two\nlines\""), "newline: quoted");
    CHECK(csv_escape("abcdef", out, 4) == -1, "too small a buffer reported");
    CHECK(csv_escape("", out, sizeof out) == 0, "empty field");
}

static void test_announcements(void)
{
    AnnouncementRec a, list[8];
    int skipped = 0, n;
    printf("Announcements\n");
    ann_store_reset();
    memset(&a, 0, sizeof a);
    strcpy(a.id, "AN001"); strcpy(a.title, "Welcome | all"); strcpy(a.body, "Line one\nLine two");
    a.audience = AUD_ALL;
    CHECK(ann_insert(&a) == DSA_OK && ann_insert(&a) == DSA_ERR_DUPLICATE, "insert once");
    strcpy(a.id, "AN002"); a.audience = AUD_COMMITTEE; strcpy(a.committeeId, "COM001"); a.pinned = 1;
    ann_insert(&a);
    a.audience = 9; strcpy(a.id, "AN003");
    CHECK(ann_insert(&a) == DSA_ERR_INVALID, "bad audience refused");
    n = ann_list(list, 8);
    CHECK(n == 2 && !strcmp(list[0].id, "AN002"), "newest first");
    CHECK(ann_save("tests/tmp_ann.txt") == DSA_OK, "save");
    ann_store_reset();
    CHECK(ann_load("tests/tmp_ann.txt", &skipped) == DSA_OK && ann_count() == 2 && skipped == 0, "load");
    ann_list(list, 8);
    CHECK(!strcmp(list[0].id, "AN002") && list[0].pinned == 1 && !strcmp(list[1].body, "Line one\nLine two"),
          "order, flags and multi-line text round-trip");
    CHECK(ann_delete("AN001") == DSA_OK && ann_count() == 1 && ann_max_id_suffix("AN") == 2, "delete");
    ann_store_reset();
    remove("tests/tmp_ann.txt");
}

int main(void)
{
    printf("=== Phase 6 C tests: audit log, statistics, CSV, announcements ===\n");
    test_audit();
    test_stats();
    test_csv();
    test_announcements();
    printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
