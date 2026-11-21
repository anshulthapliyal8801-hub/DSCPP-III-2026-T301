/*
 * MUNSphere — Module 3: Committee Management (C algorithms)
 *
 * A committee's roster is not stored separately (that would duplicate
 * delegate data). The C++ layer builds an array of RosterEntry from the
 * delegate hash table, and these functions sort and search that array.
 *
 *   roster_sort            insertion sort, stable, counts comparisons and
 *                          element moves. Insertion sort suits rosters: they
 *                          are small (under ~200) and often already nearly
 *                          sorted, where it approaches O(n).
 *   roster_find_portfolio  linear search used to stop two delegates in the
 *                          same committee from holding the same portfolio.
 */
#include <ctype.h>
#include <string.h>

#include "dsa_api.h"

static int ci_cmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || !ca) return ca - cb;
    }
}

static int entry_cmp(const RosterEntry *a, const RosterEntry *b, int key)
{
    int c = 0;
    if (key == ROSTER_BY_NAME) c = ci_cmp(a->name, b->name);
    else if (key == ROSTER_BY_PORTFOLIO) c = ci_cmp(a->portfolio, b->portfolio);
    if (c == 0) c = strcmp(a->id, b->id);          /* deterministic tie-break */
    return c;
}

void roster_sort(RosterEntry *a, int n, int key, SortStats *stats)
{
    int i, j;
    long cmp = 0, moves = 0;
    RosterEntry current;
    if (!a) n = 0;
    for (i = 1; i < n; i++) {
        current = a[i];
        j = i - 1;
        while (j >= 0) {
            cmp++;
            if (entry_cmp(&a[j], &current, key) <= 0) break;
            a[j + 1] = a[j];                           /* shift right */
            moves++;
            j--;
        }
        if (j + 1 != i) { a[j + 1] = current; moves++; }
    }
    if (stats) { stats->comparisons = cmp; stats->moves = moves; }
}

int roster_find_portfolio(const RosterEntry *a, int n, const char *portfolio,
                          const char *ignoreId)
{
    int i;
    if (!a || !portfolio || !*portfolio) return -1;
    for (i = 0; i < n; i++) {
        if (ignoreId && strcmp(a[i].id, ignoreId) == 0) continue;
        if (ci_cmp(a[i].portfolio, portfolio) == 0) return i;
    }
    return -1;
}
