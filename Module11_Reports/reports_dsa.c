/*
 * MUNSphere — Module 11: Reports and Analytics (C)
 *
 *   stats_summary   one pass for count/min/max/sum; mean and population
 *                   standard deviation in integer arithmetic (values are
 *                   percentages x 10); median from an insertion-sorted copy.
 *                   The square root is Newton's method on integers, so the
 *                   C code needs no maths library.
 *   stats_histogram counts values into fixed-width buckets (an array).
 *   csv_escape      RFC 4180 quoting for CSV export.
 *   announcements   singly linked list, newest at the head.
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

/* floor(sqrt(v)) for v >= 0, Newton's method on integers */
static long long isqrt(long long v)
{
    long long x, y;
    if (v < 2) return v;
    x = v;
    y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + v / x) / 2; }
    return x;
}

int stats_summary(const int *values, int n, StatSummary *out)
{
    int i, j, *sorted;
    long long sum = 0, sq = 0, meanX10, var;
    if (!out) return DSA_ERR_INVALID;
    memset(out, 0, sizeof *out);
    if (n <= 0 || !values) return DSA_OK;
    out->count = n;
    out->min = out->max = values[0];
    for (i = 0; i < n; i++) {
        if (values[i] < out->min) out->min = values[i];
        if (values[i] > out->max) out->max = values[i];
        sum += values[i];
    }
    out->sum = (long)sum;
    meanX10 = (sum * 10 + (sum >= 0 ? n / 2 : -n / 2)) / n;        /* rounded */
    out->meanTenths = (int)meanX10;
    /* variance x 100 = sum((10*v - 10*mean)^2) / n, using the exact mean */
    for (i = 0; i < n; i++) {
        long long d = (long long)values[i] * 10 * n - sum * 10;     /* = 10*n*(v - mean) */
        sq += d / n * (d / n);
    }
    var = sq / n;                                                   /* (10*(v-mean))^2 averaged */
    out->stddevTenths = (int)isqrt(var);

    sorted = (int *)malloc((size_t)n * sizeof *sorted);
    if (!sorted) return DSA_ERR_NO_MEMORY;
    memcpy(sorted, values, (size_t)n * sizeof *sorted);
    for (i = 1; i < n; i++) {                                       /* insertion sort */
        int cur = sorted[i];
        j = i - 1;
        while (j >= 0 && sorted[j] > cur) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = cur;
    }
    out->medianTenths = n % 2 ? sorted[n / 2] * 10 : (sorted[n / 2 - 1] + sorted[n / 2]) * 5;
    free(sorted);
    return DSA_OK;
}

int stats_histogram(const int *values, int n, int lo, int width, int *buckets, int nb)
{
    int i;
    if (!buckets || nb < 1 || width < 1) return DSA_ERR_INVALID;
    for (i = 0; i < nb; i++) buckets[i] = 0;
    if (!values) return DSA_OK;
    for (i = 0; i < n; i++) {
        int b = values[i] < lo ? 0 : (values[i] - lo) / width;
        if (b >= nb) b = nb - 1;
        buckets[b]++;
    }
    return DSA_OK;
}

int csv_escape(const char *in, char *out, size_t cap)
{
    size_t k = 0;
    const char *p;
    int quote = 0;
    if (!in || !out || cap == 0) return -1;
    for (p = in; *p; p++)
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') { quote = 1; break; }
    if (quote) { if (k + 1 >= cap) return -1; out[k++] = '"'; }
    for (p = in; *p; p++) {
        if (*p == '"') { if (k + 1 >= cap) return -1; out[k++] = '"'; }   /* "" inside quotes */
        if (k + 1 >= cap) return -1;
        out[k++] = *p;
    }
    if (quote) { if (k + 1 >= cap) return -1; out[k++] = '"'; }
    out[k] = '\0';
    return (int)k;
}

/* --------------------------------------------------------- announcements */

typedef struct AnnNode { AnnouncementRec data; struct AnnNode *next; } AnnNode;
static AnnNode *g_ann;
static int g_annCount;

void ann_store_reset(void)
{
    AnnNode *n = g_ann, *next;
    while (n) { next = n->next; free(n); n = next; }
    g_ann = NULL;
    g_annCount = 0;
}

int ann_insert(const AnnouncementRec *a)
{
    AnnNode *n;
    if (!a || !a->id[0] || !a->title[0] || a->audience < AUD_ALL || a->audience > AUD_COMMITTEE) return DSA_ERR_INVALID;
    if (ann_find(a->id, NULL) == DSA_OK) return DSA_ERR_DUPLICATE;
    n = (AnnNode *)calloc(1, sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *a;
    n->next = g_ann;
    g_ann = n;
    g_annCount++;
    return DSA_OK;
}

int ann_delete(const char *id)
{
    AnnNode *prev = NULL, *n;
    if (!id) return DSA_ERR_INVALID;
    for (n = g_ann; n; prev = n, n = n->next) {
        if (strcmp(n->data.id, id) != 0) continue;
        if (prev) prev->next = n->next; else g_ann = n->next;
        free(n);
        g_annCount--;
        return DSA_OK;
    }
    return DSA_ERR_NOT_FOUND;
}

int ann_find(const char *id, AnnouncementRec *out)
{
    AnnNode *n;
    if (!id) return DSA_ERR_INVALID;
    for (n = g_ann; n; n = n->next)
        if (strcmp(n->data.id, id) == 0) { if (out) *out = n->data; return DSA_OK; }
    return DSA_ERR_NOT_FOUND;
}

int ann_list(AnnouncementRec *out, int max)
{
    AnnNode *n;
    int k = 0;
    if (!out) return 0;
    for (n = g_ann; n && k < max; n = n->next) out[k++] = n->data;
    return k;
}

int ann_count(void) { return g_annCount; }

int ann_max_id_suffix(const char *prefix)
{
    AnnNode *n;
    int best = 0;
    size_t pl;
    if (!prefix) return 0;
    pl = strlen(prefix);
    for (n = g_ann; n; n = n->next) {
        const char *p = n->data.id;
        long v = 0;
        if (strncmp(p, prefix, pl) != 0 || !p[pl]) continue;
        for (p += pl; *p >= '0' && *p <= '9' && v < 1000000000L; p++) v = v * 10 + (*p - '0');
        if (!*p && v > best) best = (int)v;
    }
    return best;
}

int ann_save(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    AnnNode *n;
    FILE *f = pers_begin_save(path, "announcements", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (n = g_ann; n; n = n->next) {          /* newest first */
        const AnnouncementRec *a = &n->data;
        pers_write_field(f, a->id); fputc('|', f);
        pers_write_field(f, a->title); fputc('|', f);
        pers_write_field(f, a->body); fputc('|', f);
        pers_write_int(f, a->audience); fputc('|', f);
        pers_write_field(f, a->committeeId); fputc('|', f);
        pers_write_field(f, a->author); fputc('|', f);
        pers_write_field(f, a->at); fputc('|', f);
        pers_write_int(f, a->pinned); fputc('\n', f);
    }
    return pers_finish_save(f, tmp, path);
}

int ann_load(const char *path, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    AnnouncementRec *all = NULL;
    int n = 0, cap = 0, i, bad = 0, r, rc;
    FILE *f;
    if ((rc = pers_open_for_load(path, "announcements", &f)) != DSA_OK) return rc;
    ann_store_reset();
    if (!f) return DSA_OK;
    while ((r = pers_read_line(f, line, sizeof line)) != 0) {
        AnnouncementRec a;
        if (r < 0) { bad++; continue; }
        if (!line[0]) continue;
        memset(&a, 0, sizeof a);
        if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 8) { bad++; continue; }
        pers_copy_field(a.id, fld[0], DSA_ID_LEN);
        pers_copy_field(a.title, fld[1], ANN_TITLE_LEN);
        pers_copy_field(a.body, fld[2], ANN_BODY_LEN);
        a.audience = pers_parse_int(fld[3]);
        pers_copy_field(a.committeeId, fld[4], DSA_ID_LEN);
        pers_copy_field(a.author, fld[5], DSA_ID_LEN);
        pers_copy_field(a.at, fld[6], DSA_STAMP_LEN);
        a.pinned = pers_parse_int(fld[7]) == 1;
        if (n == cap) {
            AnnouncementRec *bigger;
            cap = cap ? cap * 2 : 16;
            bigger = (AnnouncementRec *)realloc(all, (size_t)cap * sizeof *bigger);
            if (!bigger) break;
            all = bigger;
        }
        all[n++] = a;
    }
    fclose(f);
    for (i = n - 1; i >= 0; i--)              /* oldest first keeps newest at the head */
        if (ann_insert(&all[i]) != DSA_OK) bad++;
    free(all);
    if (skipped) *skipped += bad;
    return DSA_OK;
}
