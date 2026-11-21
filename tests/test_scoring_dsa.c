/*
 * MUNSphere — C tests for Module 8 (scoring) and Module 9 (five sorts, awards).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsa_api.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { g_pass++; }                                           \
        else { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, msg); } \
    } while (0)

static CriterionRec crit(const char *key, const char *name, int max, int order, int att)
{
    CriterionRec c;
    memset(&c, 0, sizeof c);
    strcpy(c.key, key); strcpy(c.name, name);
    c.maxMarks = max; c.order = order; c.fromAttendance = att;
    return c;
}

static MarksheetRec sheet(const char *id, const char *delegate, int m1, int m2)
{
    MarksheetRec m;
    memset(&m, 0, sizeof m);
    strcpy(m.id, id); strcpy(m.delegateId, delegate); strcpy(m.committeeId, "COM001");
    m.criteriaCount = 2;
    strcpy(m.critKey[0], "research"); strcpy(m.critName[0], "Research | depth"); m.critMax[0] = 20; m.marks[0] = m1;
    strcpy(m.critKey[1], "speaking"); strcpy(m.critName[1], "Speaking"); m.critMax[1] = 30; m.marks[1] = m2;
    return m;
}

static void test_scoring(void)
{
    CriterionRec c[12], out[12];
    MarksheetRec m, list[8];
    SheetRevisionRec r, revs[8];
    int i, n, skipped = 0, cmp;
    printf("Criteria array and marksheets\n");
    scoring_store_reset();
    CHECK(criteria_set(c, 0) == DSA_ERR_INVALID, "no criteria refused");
    for (i = 0; i < 11; i++) { char k[16]; snprintf(k, sizeof k, "k%d", i); c[i] = crit(k, "Name", 10, i, 0); }
    CHECK(criteria_set(c, 11) == DSA_ERR_INVALID, "more than 10 refused");
    c[0] = crit("a", "A", 10, 3, 0); c[1] = crit("a", "B", 10, 1, 0);
    CHECK(criteria_set(c, 2) == DSA_ERR_DUPLICATE, "duplicate key refused");
    c[1] = crit("b", "B", 0, 1, 0);
    CHECK(criteria_set(c, 2) == DSA_ERR_INVALID, "max 0 refused");
    c[1] = crit("b", "B", 10, 1, 1); c[2] = crit("c", "C", 10, 2, 1);
    CHECK(criteria_set(c, 3) == DSA_ERR_INVALID, "two attendance criteria refused");
    c[0] = crit("speaking", "Speaking", 20, 30, 0);
    c[1] = crit("research", "Research", 20, 10, 0);
    c[2] = crit("attendance", "Attendance", 10, 20, 1);
    CHECK(criteria_set(c, 3) == DSA_OK, "valid set");
    n = criteria_get(out, 12);
    CHECK(n == 3 && !strcmp(out[0].key, "research") && !strcmp(out[1].key, "attendance") && out[2].order == 3,
          "insertion-sorted by order and renumbered 1..n");

    m = sheet("MS001", "D001", 15, 25);
    CHECK(sheet_compute_total(&m) == DSA_OK && m.total == 40 && m.maxTotal == 50, "total from the marks array");
    m.marks[1] = 31;
    CHECK(sheet_compute_total(&m) == DSA_ERR_INVALID, "mark above criterion max refused");
    m.marks[1] = -1;
    CHECK(sheet_insert(&m) == DSA_ERR_INVALID, "negative mark refused on insert");
    m = sheet("MS001", "D001", 15, 25);
    CHECK(sheet_insert(&m) == DSA_OK, "insert");
    m = sheet("MS002", "D001", 1, 1);
    CHECK(sheet_insert(&m) == DSA_ERR_DUPLICATE, "one marksheet per delegate");
    for (i = 2; i <= 40; i++) {                     /* grows the array */
        char id[16], d[16];
        snprintf(id, sizeof id, "MS%03d", i); snprintf(d, sizeof d, "D%03d", i);
        m = sheet(id, d, i % 21, i % 31);
        m.status = i % 2 ? SHEET_SUBMITTED : SHEET_DRAFT;
        sheet_insert(&m);
    }
    CHECK(sheet_count() == 40, "40 marksheets");
    CHECK(sheet_find("MS040", &m, &cmp) == DSA_OK && cmp == 40, "linear search reaches the end");
    CHECK(sheet_find_by_delegate("D007", &m) == DSA_OK && !strcmp(m.id, "MS007"), "find by delegate");
    m.marks[0] = 20; m.status = SHEET_APPROVED;
    CHECK(sheet_update(&m) == DSA_OK, "update");
    sheet_find("MS007", &m, NULL);
    CHECK(m.total == 20 + 7 && m.status == SHEET_APPROVED, "update recomputes the total");
    CHECK(sheet_list("COM001", SHEET_SUBMITTED, list, 8) == 8, "filter by status (capped at max)");
    CHECK(sheet_max_id_suffix("MS") == 40, "id suffix");

    memset(&r, 0, sizeof r);
    strcpy(r.marksheetId, "MS001");
    for (i = 0; i < 3; i++) { r.revision = i + 1; r.action = i; r.marksCount = 2; r.marks[0] = i; rev_add(&r); }
    strcpy(r.marksheetId, "MS002"); rev_add(&r);
    n = rev_list("MS001", revs, 8);
    CHECK(n == 3 && revs[0].revision == 1 && revs[2].action == SHEET_ACT_SUBMITTED, "history oldest first");
    r.action = 99;
    CHECK(rev_add(&r) == DSA_ERR_INVALID, "bad action refused");

    CHECK(scoring_save("tests/tmp_c.txt", "tests/tmp_s.txt", "tests/tmp_r.txt") == DSA_OK, "save");
    scoring_store_reset();
    CHECK(scoring_load("tests/tmp_c.txt", "tests/tmp_s.txt", "tests/tmp_r.txt", &skipped) == DSA_OK && skipped == 0, "load");
    CHECK(criteria_count() == 3 && sheet_count() == 40 && rev_count() == 4, "everything reloaded");
    sheet_find("MS001", &m, NULL);
    CHECK(!strcmp(m.critName[0], "Research | depth") && m.marks[1] == 25 && m.total == 40, "snapshot round-trips");
    remove("tests/tmp_c.txt"); remove("tests/tmp_s.txt"); remove("tests/tmp_r.txt");
}

static int qcmp(const void *a, const void *b) { return rank_compare((const RankEntry *)a, (const RankEntry *)b); }

static void fill(RankEntry *a, int n, int mode, unsigned seed)
{
    int i;
    srand(seed);
    for (i = 0; i < n; i++) {
        memset(&a[i], 0, sizeof a[i]);
        sprintf(a[i].delegateId, "D%05d", i);
        if (mode == 0) {            /* random with many ties to exercise the tie-breaks */
            a[i].percentTenths = 500 + (rand() % 20) * 10;
            a[i].tiebreakTenths = (rand() % 4) * 100;
            a[i].attendanceTenths = (rand() % 3) * 250;
        } else {                    /* strictly ordered */
            a[i].percentTenths = mode == 1 ? 1000 - i : i;
        }
    }
    if (mode == 3) for (i = 0; i < n; i++) a[i].percentTenths = 700;   /* all tied */
}

static void test_sorts(void)
{
    static RankEntry base[5000], work[5000], ref[5000];
    int sizes[] = {0, 1, 2, 7, 500};
    int modes[] = {0, 1, 2, 3};
    int alg, si, mi, n, ok;
    SortStats st;
    printf("Five sorting algorithms against qsort\n");
    for (si = 0; si < 5; si++)
        for (mi = 0; mi < 4; mi++) {
            n = sizes[si];
            fill(base, n, modes[mi], 11u + (unsigned)si);
            memcpy(ref, base, (size_t)n * sizeof *ref);
            qsort(ref, (size_t)n, sizeof *ref, qcmp);
            ok = 1;
            for (alg = 0; alg < SORT_ALGORITHM_COUNT; alg++) {
                memcpy(work, base, (size_t)n * sizeof *work);
                rank_sort(work, n, alg, &st);
                if (n && memcmp(work, ref, (size_t)n * sizeof *ref) != 0) ok = 0;
            }
            if (!ok) printf("  mismatch: n=%d mode=%d\n", n, modes[mi]);
            CHECK(ok, "all five match qsort");
        }

    printf("Measured work matches the textbook\n");
    n = 1000;
    fill(base, n, 1, 1);                            /* already in rank order */
    memcpy(work, base, sizeof(RankEntry) * (size_t)n); rank_sort(work, n, SORT_BUBBLE, &st);
    CHECK(st.comparisons == n - 1 && st.moves == 0, "bubble on sorted: n-1 comparisons, 0 swaps");
    memcpy(work, base, sizeof(RankEntry) * (size_t)n); rank_sort(work, n, SORT_INSERTION, &st);
    CHECK(st.comparisons == n - 1 && st.moves == 0, "insertion on sorted: n-1 comparisons, 0 moves");
    memcpy(work, base, sizeof(RankEntry) * (size_t)n); rank_sort(work, n, SORT_SELECTION, &st);
    CHECK(st.comparisons == (long)n * (n - 1) / 2 && st.moves == 0, "selection: n(n-1)/2 comparisons even when sorted");
    memcpy(work, base, sizeof(RankEntry) * (size_t)n); rank_sort(work, n, SORT_QUICK, &st);
    CHECK(st.comparisons == (long)n * (n - 1) / 2, "quick, last-element pivot, sorted input: n(n-1)/2 (worst case)");
    fill(base, n, 2, 1);                            /* reverse order */
    memcpy(work, base, sizeof(RankEntry) * (size_t)n); rank_sort(work, n, SORT_BUBBLE, &st);
    CHECK(st.comparisons == (long)n * (n - 1) / 2 && st.moves == (long)n * (n - 1) / 2, "bubble on reversed: n(n-1)/2 swaps");
    fill(base, 1024, 0, 5);
    memcpy(work, base, sizeof(RankEntry) * 1024); rank_sort(work, 1024, SORT_MERGE, &st);
    CHECK(st.comparisons <= 1024L * 10 && st.moves == 2L * 1024 * 10, "merge: <= n log n comparisons, 2 n log n copies");
    memcpy(work, base, sizeof(RankEntry) * 1024); rank_sort(work, 1024, SORT_QUICK, &st);
    CHECK(st.comparisons < 1024L * 10 * 3, "quick on random data stays near n log n");
    fill(base, 5000, 1, 1);
    rank_sort(base, 5000, SORT_QUICK, &st);
    CHECK(st.comparisons == 5000L * 4999 / 2, "quick worst case at n=5000 completes (shallow stack)");
    CHECK(rank_sort(base, 3, 9, &st) == DSA_ERR_INVALID, "unknown algorithm refused");
    CHECK(!strcmp(sort_algorithm_name(SORT_MERGE), "Merge sort"), "names");
}

static void test_awards(void)
{
    AwardRule r[3], out[8];
    char tb[DSA_ID_LEN];
    int skipped = 0;
    printf("Award rules\n");
    awards_store_reset();
    memset(r, 0, sizeof r);
    strcpy(r[0].name, "Best Delegate"); r[0].positions = 1; r[0].minPercent = 70;
    strcpy(r[1].name, "High | Commendation"); r[1].positions = 2; r[1].minPercent = 60;
    CHECK(awards_set(r, 2, "speaking") == DSA_OK, "set");
    r[1].minPercent = 101;
    CHECK(awards_set(r, 2, "x") == DSA_ERR_INVALID, "threshold above 100 refused");
    r[1] = r[0];
    CHECK(awards_set(r, 2, "x") == DSA_ERR_DUPLICATE, "duplicate award name refused");
    CHECK(awards_save("tests/tmp_a.txt") == DSA_OK, "save");
    awards_store_reset();
    CHECK(awards_load("tests/tmp_a.txt", &skipped) == DSA_OK && skipped == 0 && awards_count() == 2, "load");
    awards_get(out, 8);
    awards_tiebreak(tb);
    CHECK(!strcmp(out[1].name, "High | Commendation") && out[1].positions == 2 && !strcmp(tb, "speaking"), "round-trip");
    remove("tests/tmp_a.txt");
}

int main(void)
{
    printf("=== Phase 5 C tests: scoring, sorting, awards ===\n");
    test_scoring();
    test_sorts();
    test_awards();
    scoring_store_reset();
    awards_store_reset();
    printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
