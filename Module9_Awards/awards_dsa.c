/*
 * MUNSphere — Module 9: Awards and Ranking (C)
 *
 * Five sorting algorithms over the same RankEntry array. Each counts:
 *   comparisons : calls to rank_compare
 *   moves       : swaps (bubble, selection, quick) or element copies
 *                 (insertion shifts, merge copies)
 *
 *   Bubble     O(n^2), O(n) when already sorted (stops after a clean pass). Stable.
 *   Selection  O(n^2) comparisons always, at most n-1 swaps. Not stable.
 *   Insertion  O(n^2) worst, O(n) when nearly sorted. Stable.
 *   Merge      O(n log n) always, O(n) extra memory. Stable.
 *   Quick      O(n log n) average; O(n^2) on sorted input with this
 *              textbook last-element pivot (shown on purpose). Not stable.
 *              Recursing into the smaller part and looping on the larger
 *              keeps the stack depth O(log n) even in that worst case.
 *
 * rank_compare is a TOTAL order (it ends with the unique Delegate ID), so
 * stable and unstable algorithms produce exactly the same ranking.
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

int rank_compare(const RankEntry *a, const RankEntry *b)
{
    if (a->percentTenths != b->percentTenths) return b->percentTenths - a->percentTenths;   /* higher first */
    if (a->tiebreakTenths != b->tiebreakTenths) return b->tiebreakTenths - a->tiebreakTenths;
    if (a->attendanceTenths != b->attendanceTenths) return b->attendanceTenths - a->attendanceTenths;
    return strcmp(a->delegateId, b->delegateId);
}

static long g_cmp, g_moves;

static int cmp(const RankEntry *a, const RankEntry *b)
{
    g_cmp++;
    return rank_compare(a, b);
}

static void swap(RankEntry *a, RankEntry *b)
{
    RankEntry t = *a;
    *a = *b;
    *b = t;
    g_moves++;
}

static void bubble_sort(RankEntry *a, int n)
{
    int i, j, swapped;
    for (i = 0; i < n - 1; i++) {
        swapped = 0;
        for (j = 0; j < n - 1 - i; j++)
            if (cmp(&a[j], &a[j + 1]) > 0) { swap(&a[j], &a[j + 1]); swapped = 1; }
        if (!swapped) break;                       /* already in order */
    }
}

static void selection_sort(RankEntry *a, int n)
{
    int i, j, best;
    for (i = 0; i < n - 1; i++) {
        best = i;
        for (j = i + 1; j < n; j++)
            if (cmp(&a[j], &a[best]) < 0) best = j;
        if (best != i) swap(&a[i], &a[best]);
    }
}

static void insertion_sort(RankEntry *a, int n)
{
    int i, j;
    RankEntry cur;
    for (i = 1; i < n; i++) {
        cur = a[i];
        j = i - 1;
        while (j >= 0 && cmp(&a[j], &cur) > 0) { a[j + 1] = a[j]; g_moves++; j--; }
        if (j + 1 != i) { a[j + 1] = cur; g_moves++; }
    }
}

static void merge_rec(RankEntry *a, RankEntry *tmp, int lo, int hi)
{
    int mid, i, j, k;
    if (hi - lo < 1) return;
    mid = lo + (hi - lo) / 2;
    merge_rec(a, tmp, lo, mid);
    merge_rec(a, tmp, mid + 1, hi);
    i = lo; j = mid + 1; k = lo;
    while (i <= mid && j <= hi) {
        if (cmp(&a[i], &a[j]) <= 0) tmp[k++] = a[i++];   /* <= keeps it stable */
        else tmp[k++] = a[j++];
        g_moves++;
    }
    while (i <= mid) { tmp[k++] = a[i++]; g_moves++; }
    while (j <= hi) { tmp[k++] = a[j++]; g_moves++; }
    for (k = lo; k <= hi; k++) { a[k] = tmp[k]; g_moves++; }
}

/* Lomuto partition around the last element. */
static int partition(RankEntry *a, int lo, int hi)
{
    int i = lo - 1, j;
    for (j = lo; j < hi; j++)
        if (cmp(&a[j], &a[hi]) < 0) { i++; if (i != j) swap(&a[i], &a[j]); }
    if (i + 1 != hi) swap(&a[i + 1], &a[hi]);
    return i + 1;
}

static void quick_rec(RankEntry *a, int lo, int hi)
{
    while (lo < hi) {
        int p = partition(a, lo, hi);
        if (p - lo < hi - p) { quick_rec(a, lo, p - 1); lo = p + 1; }   /* recurse on smaller part */
        else { quick_rec(a, p + 1, hi); hi = p - 1; }
    }
}

int rank_sort(RankEntry *a, int n, int algorithm, SortStats *stats)
{
    g_cmp = 0;
    g_moves = 0;
    if (n < 0 || (n > 0 && !a)) return DSA_ERR_INVALID;
    switch (algorithm) {
    case SORT_BUBBLE: bubble_sort(a, n); break;
    case SORT_SELECTION: selection_sort(a, n); break;
    case SORT_INSERTION: insertion_sort(a, n); break;
    case SORT_MERGE: {
        RankEntry *tmp;
        if (n < 2) break;
        tmp = (RankEntry *)malloc((size_t)n * sizeof *tmp);
        if (!tmp) return DSA_ERR_NO_MEMORY;
        merge_rec(a, tmp, 0, n - 1);
        free(tmp);
        break;
    }
    case SORT_QUICK: if (n > 1) quick_rec(a, 0, n - 1); break;
    default: return DSA_ERR_INVALID;
    }
    if (stats) { stats->comparisons = g_cmp; stats->moves = g_moves; }
    return DSA_OK;
}

const char *sort_algorithm_name(int algorithm)
{
    static const char *const names[SORT_ALGORITHM_COUNT] = {
        "Bubble sort", "Selection sort", "Insertion sort", "Merge sort", "Quick sort"};
    return algorithm >= 0 && algorithm < SORT_ALGORITHM_COUNT ? names[algorithm] : "Unknown";
}

/* ---------------------------------------------------------- award rules */

static AwardRule g_rules[MAX_AWARDS];
static int g_ruleCount;
static char g_tiebreak[DSA_ID_LEN];

void awards_store_reset(void) { g_ruleCount = 0; g_tiebreak[0] = '\0'; }

int awards_set(const AwardRule *rules, int n, const char *tiebreakKey)
{
    int i, j;
    if (!rules || n < 1 || n > MAX_AWARDS) return DSA_ERR_INVALID;
    for (i = 0; i < n; i++) {
        if (!rules[i].name[0] || rules[i].positions < 1 || rules[i].positions > 50 ||
            rules[i].minPercent < 0 || rules[i].minPercent > 100)
            return DSA_ERR_INVALID;
        for (j = 0; j < i; j++)
            if (strcmp(rules[j].name, rules[i].name) == 0) return DSA_ERR_DUPLICATE;
    }
    for (i = 0; i < n; i++) { g_rules[i] = rules[i]; g_rules[i].name[AWARD_NAME_LEN - 1] = '\0'; }
    g_ruleCount = n;
    pers_copy_field(g_tiebreak, tiebreakKey ? tiebreakKey : "", DSA_ID_LEN);
    return DSA_OK;
}

int awards_get(AwardRule *out, int max)
{
    int i;
    if (!out) return 0;
    for (i = 0; i < g_ruleCount && i < max; i++) out[i] = g_rules[i];
    return i;
}

int awards_count(void) { return g_ruleCount; }
void awards_tiebreak(char *keyOut) { if (keyOut) pers_copy_field(keyOut, g_tiebreak, DSA_ID_LEN); }

int awards_save(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    int i;
    FILE *f = pers_begin_save(path, "award_rules", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    fputs("tiebreak|", f);
    pers_write_field(f, g_tiebreak);
    fputc('\n', f);
    for (i = 0; i < g_ruleCount; i++) {
        fputs("rule|", f);
        pers_write_field(f, g_rules[i].name); fputc('|', f);
        pers_write_int(f, g_rules[i].positions); fputc('|', f);
        pers_write_int(f, g_rules[i].minPercent); fputc('\n', f);
    }
    return pers_finish_save(f, tmp, path);
}

int awards_load(const char *path, int *skipped)
{
    char line[PERS_LINE_MAX], tiebreak[DSA_ID_LEN] = "";
    char *fld[PERS_MAX_FIELDS];
    AwardRule rules[MAX_AWARDS];
    int n = 0, bad = 0, r, rc;
    FILE *f;
    if ((rc = pers_open_for_load(path, "award_rules", &f)) != DSA_OK) return rc;
    awards_store_reset();
    if (!f) return DSA_OK;
    while ((r = pers_read_line(f, line, sizeof line)) != 0) {
        int k;
        if (r < 0) { bad++; continue; }
        if (!line[0]) continue;
        k = pers_split_fields(line, fld, PERS_MAX_FIELDS);
        if (k == 2 && strcmp(fld[0], "tiebreak") == 0) { pers_copy_field(tiebreak, fld[1], DSA_ID_LEN); continue; }
        if (k != 4 || strcmp(fld[0], "rule") != 0 || n == MAX_AWARDS) { bad++; continue; }
        memset(&rules[n], 0, sizeof rules[n]);
        pers_copy_field(rules[n].name, fld[1], AWARD_NAME_LEN);
        rules[n].positions = pers_parse_int(fld[2]);
        rules[n].minPercent = pers_parse_int(fld[3]);
        n++;
    }
    fclose(f);
    if (n && awards_set(rules, n, tiebreak) != DSA_OK) bad += n;
    if (skipped) *skipped += bad;
    return DSA_OK;
}
