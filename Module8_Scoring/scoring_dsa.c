/*
 * MUNSphere — Module 8: Delegate Scoring (C)
 *
 *   CRITERIA   fixed array of at most MAX_CRITERIA structures, kept in
 *              display order with insertion sort on `order`.
 *   MARKSHEETS dynamic array (doubles when full); linear search by ID or
 *              by delegate. Each marksheet carries a SNAPSHOT of the
 *              criteria it was marked against, as parallel arrays
 *              (critKey[i], critName[i], critMax[i], marks[i]), so later
 *              changes to the criteria never alter an old marksheet.
 *   REVISIONS  singly linked list with head and tail; append O(1), the
 *              history of a marksheet reads oldest to newest.
 *   TOTAL      one pass over the marks array, checking each mark against
 *              its criterion's maximum.
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

typedef struct RevNode { SheetRevisionRec data; struct RevNode *next; } RevNode;

static CriterionRec g_criteria[MAX_CRITERIA];
static int g_critCount;
static MarksheetRec *g_sheets;
static int g_sheetCount, g_sheetCap;
static RevNode *g_revHead, *g_revTail;
static int g_revCount;

void scoring_store_reset(void)
{
    RevNode *r = g_revHead, *n;
    while (r) { n = r->next; free(r); r = n; }
    g_revHead = g_revTail = NULL;
    g_revCount = 0;
    free(g_sheets);
    g_sheets = NULL;
    g_sheetCount = g_sheetCap = 0;
    g_critCount = 0;
}

/* ------------------------------------------------------------ criteria */

int criteria_set(const CriterionRec *arr, int n)
{
    CriterionRec tmp[MAX_CRITERIA], cur;
    int i, j, attendance = 0;
    if (!arr || n < 1 || n > MAX_CRITERIA) return DSA_ERR_INVALID;
    for (i = 0; i < n; i++) {
        tmp[i] = arr[i];
        tmp[i].key[DSA_ID_LEN - 1] = '\0';
        tmp[i].name[CRIT_NAME_LEN - 1] = '\0';
        if (!tmp[i].key[0] || !tmp[i].name[0]) return DSA_ERR_INVALID;
        if (tmp[i].maxMarks < 1 || tmp[i].maxMarks > 100) return DSA_ERR_INVALID;
        if (tmp[i].fromAttendance) attendance++;
        for (j = 0; j < i; j++)
            if (strcmp(tmp[j].key, tmp[i].key) == 0) return DSA_ERR_DUPLICATE;
    }
    if (attendance > 1) return DSA_ERR_INVALID;
    for (i = 1; i < n; i++) {                       /* insertion sort by order */
        cur = tmp[i];
        j = i - 1;
        while (j >= 0 && tmp[j].order > cur.order) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = cur;
    }
    for (i = 0; i < n; i++) { tmp[i].order = i + 1; g_criteria[i] = tmp[i]; }
    g_critCount = n;
    return DSA_OK;
}

int criteria_get(CriterionRec *out, int max)
{
    int i;
    if (!out) return 0;
    for (i = 0; i < g_critCount && i < max; i++) out[i] = g_criteria[i];
    return i;
}

int criteria_count(void) { return g_critCount; }

/* ----------------------------------------------------------- marksheets */

int sheet_compute_total(MarksheetRec *m)
{
    int i, total = 0, maxTotal = 0;
    if (!m || m->criteriaCount < 1 || m->criteriaCount > MAX_CRITERIA) return DSA_ERR_INVALID;
    for (i = 0; i < m->criteriaCount; i++) {
        if (m->marks[i] < 0 || m->marks[i] > m->critMax[i]) return DSA_ERR_INVALID;
        total += m->marks[i];
        maxTotal += m->critMax[i];
    }
    m->total = total;
    m->maxTotal = maxTotal;
    return DSA_OK;
}

static int sheet_index(const char *id, int *cmpOut)
{
    int i, cmp = 0;
    for (i = 0; i < g_sheetCount; i++) {
        cmp++;
        if (strcmp(g_sheets[i].id, id) == 0) { if (cmpOut) *cmpOut = cmp; return i; }
    }
    if (cmpOut) *cmpOut = cmp;
    return -1;
}

int sheet_insert(const MarksheetRec *m)
{
    MarksheetRec copy;
    int i;
    if (!m || !m->id[0] || !m->delegateId[0]) return DSA_ERR_INVALID;
    if (m->status < 0 || m->status >= SHEET_STATUS_COUNT) return DSA_ERR_INVALID;
    if (sheet_index(m->id, NULL) >= 0) return DSA_ERR_DUPLICATE;
    for (i = 0; i < g_sheetCount; i++)            /* one marksheet per delegate */
        if (strcmp(g_sheets[i].delegateId, m->delegateId) == 0) return DSA_ERR_DUPLICATE;
    copy = *m;
    if (sheet_compute_total(&copy) != DSA_OK) return DSA_ERR_INVALID;
    if (g_sheetCount == g_sheetCap) {
        int cap = g_sheetCap ? g_sheetCap * 2 : 16;
        MarksheetRec *bigger = (MarksheetRec *)realloc(g_sheets, (size_t)cap * sizeof *bigger);
        if (!bigger) return DSA_ERR_NO_MEMORY;
        g_sheets = bigger;
        g_sheetCap = cap;
    }
    g_sheets[g_sheetCount++] = copy;
    return DSA_OK;
}

int sheet_find(const char *id, MarksheetRec *out, int *comparisons)
{
    int i;
    if (!id) return DSA_ERR_INVALID;
    i = sheet_index(id, comparisons);
    if (i < 0) return DSA_ERR_NOT_FOUND;
    if (out) *out = g_sheets[i];
    return DSA_OK;
}

int sheet_find_by_delegate(const char *delegateId, MarksheetRec *out)
{
    int i;
    if (!delegateId) return DSA_ERR_INVALID;
    for (i = 0; i < g_sheetCount; i++)
        if (strcmp(g_sheets[i].delegateId, delegateId) == 0) {
            if (out) *out = g_sheets[i];
            return DSA_OK;
        }
    return DSA_ERR_NOT_FOUND;
}

int sheet_update(const MarksheetRec *m)
{
    MarksheetRec copy;
    int i;
    if (!m || m->status < 0 || m->status >= SHEET_STATUS_COUNT) return DSA_ERR_INVALID;
    i = sheet_index(m->id, NULL);
    if (i < 0) return DSA_ERR_NOT_FOUND;
    copy = *m;
    if (sheet_compute_total(&copy) != DSA_OK) return DSA_ERR_INVALID;
    g_sheets[i] = copy;
    return DSA_OK;
}

int sheet_list(const char *committeeId, int status, MarksheetRec *out, int max)
{
    int i, k = 0;
    if (!out) return 0;
    for (i = 0; i < g_sheetCount && k < max; i++) {
        if (committeeId && *committeeId && strcmp(g_sheets[i].committeeId, committeeId) != 0) continue;
        if (status >= 0 && g_sheets[i].status != status) continue;
        out[k++] = g_sheets[i];
    }
    return k;
}

int sheet_count(void) { return g_sheetCount; }

int sheet_max_id_suffix(const char *prefix)
{
    int i, best = 0;
    size_t pl;
    if (!prefix) return 0;
    pl = strlen(prefix);
    for (i = 0; i < g_sheetCount; i++) {
        const char *p = g_sheets[i].id;
        long v = 0;
        if (strncmp(p, prefix, pl) != 0 || !p[pl]) continue;
        for (p += pl; *p >= '0' && *p <= '9' && v < 1000000000L; p++) v = v * 10 + (*p - '0');
        if (!*p && v > best) best = (int)v;
    }
    return best;
}

/* ------------------------------------------------------------ revisions */

int rev_add(const SheetRevisionRec *r)
{
    RevNode *n;
    if (!r || !r->marksheetId[0] || r->action < 0 || r->action >= SHEET_ACTION_COUNT) return DSA_ERR_INVALID;
    if (r->marksCount < 0 || r->marksCount > MAX_CRITERIA) return DSA_ERR_INVALID;
    n = (RevNode *)calloc(1, sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *r;
    if (g_revTail) g_revTail->next = n; else g_revHead = n;   /* append at tail */
    g_revTail = n;
    g_revCount++;
    return DSA_OK;
}

int rev_list(const char *marksheetId, SheetRevisionRec *out, int max)
{
    RevNode *n;
    int k = 0;
    if (!marksheetId || !out) return 0;
    for (n = g_revHead; n && k < max; n = n->next)
        if (strcmp(n->data.marksheetId, marksheetId) == 0) out[k++] = n->data;
    return k;
}

int rev_count(void) { return g_revCount; }

/* -------------------------------------------------------- file handling */

int scoring_save(const char *criteriaPath, const char *sheetPath, const char *revisionPath)
{
    char tmp[DSA_PATH_LEN + 8];
    FILE *f;
    int i, k, rc;
    RevNode *r;

    if (!(f = pers_begin_save(criteriaPath, "criteria", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (i = 0; i < g_critCount; i++) {
        pers_write_field(f, g_criteria[i].key); fputc('|', f);
        pers_write_field(f, g_criteria[i].name); fputc('|', f);
        pers_write_int(f, g_criteria[i].maxMarks); fputc('|', f);
        pers_write_int(f, g_criteria[i].order); fputc('|', f);
        pers_write_int(f, g_criteria[i].fromAttendance); fputc('\n', f);
    }
    if ((rc = pers_finish_save(f, tmp, criteriaPath)) != DSA_OK) return rc;

    /* 13 fixed fields, then 4 per criterion: key|name|max|mark */
    if (!(f = pers_begin_save(sheetPath, "marksheets", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (i = 0; i < g_sheetCount; i++) {
        const MarksheetRec *m = &g_sheets[i];
        pers_write_field(f, m->id); fputc('|', f);
        pers_write_field(f, m->delegateId); fputc('|', f);
        pers_write_field(f, m->committeeId); fputc('|', f);
        pers_write_int(f, m->status); fputc('|', f);
        pers_write_int(f, m->revision); fputc('|', f);
        pers_write_field(f, m->remarks); fputc('|', f);
        pers_write_field(f, m->adminNote); fputc('|', f);
        pers_write_field(f, m->createdBy); fputc('|', f);
        pers_write_field(f, m->updatedAt); fputc('|', f);
        pers_write_int(f, m->total); fputc('|', f);
        pers_write_int(f, m->maxTotal); fputc('|', f);
        pers_write_int(f, 0); fputc('|', f);      /* reserved */
        pers_write_int(f, m->criteriaCount);
        for (k = 0; k < m->criteriaCount; k++) {
            fputc('|', f); pers_write_field(f, m->critKey[k]);
            fputc('|', f); pers_write_field(f, m->critName[k]);
            fputc('|', f); pers_write_int(f, m->critMax[k]);
            fputc('|', f); pers_write_int(f, m->marks[k]);
        }
        fputc('\n', f);
    }
    if ((rc = pers_finish_save(f, tmp, sheetPath)) != DSA_OK) return rc;

    if (!(f = pers_begin_save(revisionPath, "marksheet_revisions", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (r = g_revHead; r; r = r->next) {
        const SheetRevisionRec *v = &r->data;
        pers_write_field(f, v->marksheetId); fputc('|', f);
        pers_write_int(f, v->revision); fputc('|', f);
        pers_write_int(f, v->action); fputc('|', f);
        pers_write_field(f, v->by); fputc('|', f);
        pers_write_field(f, v->at); fputc('|', f);
        pers_write_int(f, v->total); fputc('|', f);
        pers_write_field(f, v->note); fputc('|', f);
        pers_write_int(f, v->marksCount);
        for (k = 0; k < v->marksCount; k++) { fputc('|', f); pers_write_int(f, v->marks[k]); }
        fputc('\n', f);
    }
    return pers_finish_save(f, tmp, revisionPath);
}

int scoring_load(const char *criteriaPath, const char *sheetPath, const char *revisionPath, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f1, *f2, *f3;
    int bad = 0, r, rc, n, k;
    CriterionRec crit[MAX_CRITERIA];
    int critN = 0;

    if ((rc = pers_open_for_load(criteriaPath, "criteria", &f1)) != DSA_OK) return rc;
    if ((rc = pers_open_for_load(sheetPath, "marksheets", &f2)) != DSA_OK) { if (f1) fclose(f1); return rc; }
    if ((rc = pers_open_for_load(revisionPath, "marksheet_revisions", &f3)) != DSA_OK) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return rc;
    }
    scoring_store_reset();

    if (f1) {
        while ((r = pers_read_line(f1, line, sizeof line)) != 0) {
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 5 || critN == MAX_CRITERIA) { bad++; continue; }
            memset(&crit[critN], 0, sizeof crit[critN]);
            pers_copy_field(crit[critN].key, fld[0], DSA_ID_LEN);
            pers_copy_field(crit[critN].name, fld[1], CRIT_NAME_LEN);
            crit[critN].maxMarks = pers_parse_int(fld[2]);
            crit[critN].order = pers_parse_int(fld[3]);
            crit[critN].fromAttendance = pers_parse_int(fld[4]) == 1;
            critN++;
        }
        fclose(f1);
        if (critN && criteria_set(crit, critN) != DSA_OK) bad += critN;
    }
    if (f2) {
        while ((r = pers_read_line(f2, line, sizeof line)) != 0) {
            MarksheetRec m;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&m, 0, sizeof m);
            n = pers_split_fields(line, fld, PERS_MAX_FIELDS);
            if (n < 13) { bad++; continue; }
            m.criteriaCount = pers_parse_int(fld[12]);
            if (m.criteriaCount < 1 || m.criteriaCount > MAX_CRITERIA || n != 13 + 4 * m.criteriaCount) { bad++; continue; }
            pers_copy_field(m.id, fld[0], DSA_ID_LEN);
            pers_copy_field(m.delegateId, fld[1], DSA_ID_LEN);
            pers_copy_field(m.committeeId, fld[2], DSA_ID_LEN);
            m.status = pers_parse_int(fld[3]);
            m.revision = pers_parse_int(fld[4]);
            pers_copy_field(m.remarks, fld[5], DSA_TEXT_LEN);
            pers_copy_field(m.adminNote, fld[6], DSA_TEXT_LEN);
            pers_copy_field(m.createdBy, fld[7], DSA_ID_LEN);
            pers_copy_field(m.updatedAt, fld[8], DSA_STAMP_LEN);
            for (k = 0; k < m.criteriaCount; k++) {
                pers_copy_field(m.critKey[k], fld[13 + 4 * k], DSA_ID_LEN);
                pers_copy_field(m.critName[k], fld[14 + 4 * k], CRIT_NAME_LEN);
                m.critMax[k] = pers_parse_int(fld[15 + 4 * k]);
                m.marks[k] = pers_parse_int(fld[16 + 4 * k]);
            }
            if (sheet_insert(&m) != DSA_OK) bad++;    /* re-validates totals */
        }
        fclose(f2);
    }
    if (f3) {
        while ((r = pers_read_line(f3, line, sizeof line)) != 0) {
            SheetRevisionRec v;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&v, 0, sizeof v);
            n = pers_split_fields(line, fld, PERS_MAX_FIELDS);
            if (n < 8) { bad++; continue; }
            v.marksCount = pers_parse_int(fld[7]);
            if (v.marksCount < 0 || v.marksCount > MAX_CRITERIA || n != 8 + v.marksCount) { bad++; continue; }
            pers_copy_field(v.marksheetId, fld[0], DSA_ID_LEN);
            v.revision = pers_parse_int(fld[1]);
            v.action = pers_parse_int(fld[2]);
            pers_copy_field(v.by, fld[3], DSA_ID_LEN);
            pers_copy_field(v.at, fld[4], DSA_STAMP_LEN);
            v.total = pers_parse_int(fld[5]);
            pers_copy_field(v.note, fld[6], DSA_TEXT_LEN);
            for (k = 0; k < v.marksCount; k++) v.marks[k] = pers_parse_int(fld[8 + k]);
            if (rev_add(&v) != DSA_OK) bad++;
        }
        fclose(f3);
    }
    if (skipped) *skipped += bad;
    return DSA_OK;
}
