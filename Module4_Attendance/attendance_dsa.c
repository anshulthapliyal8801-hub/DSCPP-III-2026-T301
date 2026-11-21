/*
 * MUNSphere — Module 4: Attendance (C)
 *
 *   Linked list of "sheets", one per session that has any attendance.
 *   Each sheet holds a dynamic ARRAY of entries. When the array is full
 *   its capacity doubles (realloc), so appending is O(1) amortized.
 *
 *   Duplicate prevention: marking a delegate who is already on the
 *   session's sheet UPDATES that entry instead of adding a second one.
 *   Finding the entry is a linear search over the sheet's array.
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

typedef struct Sheet {
    char sessionId[DSA_ID_LEN];
    AttendanceRec *items;
    int count, capacity;
    struct Sheet *next;
} Sheet;

static Sheet *g_sheets;
static int g_total;

static Sheet *find_sheet(Sheet *head, const char *sessionId)
{
    while (head && strcmp(head->sessionId, sessionId) != 0) head = head->next;
    return head;
}

static int find_entry(const Sheet *s, const char *delegateId, int *cmpOut)
{
    int i, cmp = 0;
    for (i = 0; i < s->count; i++) {
        cmp++;
        if (strcmp(s->items[i].delegateId, delegateId) == 0) { if (cmpOut) *cmpOut = cmp; return i; }
    }
    if (cmpOut) *cmpOut = cmp;
    return -1;
}

static void free_sheets(Sheet **head)
{
    Sheet *s = *head, *n;
    while (s) { n = s->next; free(s->items); free(s); s = n; }
    *head = NULL;
}

static int mark_into(Sheet **head, int *total, const AttendanceRec *rec, int *wasUpdate)
{
    Sheet *s;
    int i;
    if (!rec || !rec->sessionId[0] || !rec->delegateId[0]) return DSA_ERR_INVALID;
    if (rec->status < 0 || rec->status >= ATT_STATUS_COUNT) return DSA_ERR_INVALID;
    s = find_sheet(*head, rec->sessionId);
    if (!s) {
        s = (Sheet *)calloc(1, sizeof *s);
        if (!s) return DSA_ERR_NO_MEMORY;
        pers_copy_field(s->sessionId, rec->sessionId, DSA_ID_LEN);
        s->next = *head;
        *head = s;
    }
    i = find_entry(s, rec->delegateId, NULL);
    if (i >= 0) {                                  /* already marked: update */
        s->items[i] = *rec;
        if (wasUpdate) *wasUpdate = 1;
        return DSA_OK;
    }
    if (s->count == s->capacity) {                 /* grow: double the array */
        int cap = s->capacity ? s->capacity * 2 : 8;
        AttendanceRec *bigger = (AttendanceRec *)realloc(s->items, (size_t)cap * sizeof *bigger);
        if (!bigger) return DSA_ERR_NO_MEMORY;
        s->items = bigger;
        s->capacity = cap;
    }
    s->items[s->count] = *rec;
    s->items[s->count].markedBy[DSA_ID_LEN - 1] = '\0';
    s->items[s->count].markedAt[DSA_STAMP_LEN - 1] = '\0';
    s->count++;
    (*total)++;
    if (wasUpdate) *wasUpdate = 0;
    return DSA_OK;
}

void att_store_reset(void) { free_sheets(&g_sheets); g_total = 0; }
int  att_total(void) { return g_total; }

int att_mark(const AttendanceRec *rec, int *wasUpdate)
{
    return mark_into(&g_sheets, &g_total, rec, wasUpdate);
}

int att_find(const char *sessionId, const char *delegateId, AttendanceRec *out, int *comparisons)
{
    Sheet *s;
    int i;
    if (comparisons) *comparisons = 0;
    if (!sessionId || !delegateId) return DSA_ERR_INVALID;
    s = find_sheet(g_sheets, sessionId);
    if (!s) return DSA_ERR_NOT_FOUND;
    i = find_entry(s, delegateId, comparisons);
    if (i < 0) return DSA_ERR_NOT_FOUND;
    if (out) *out = s->items[i];
    return DSA_OK;
}

int att_list_session(const char *sessionId, AttendanceRec *out, int max)
{
    Sheet *s;
    int i, k = 0;
    if (!sessionId || !out) return 0;
    s = find_sheet(g_sheets, sessionId);
    if (!s) return 0;
    for (i = 0; i < s->count && k < max; i++) out[k++] = s->items[i];
    return k;
}

int att_list_delegate(const char *delegateId, AttendanceRec *out, int max)
{
    Sheet *s;
    int k = 0;
    if (!delegateId || !out) return 0;
    for (s = g_sheets; s && k < max; s = s->next) {
        int i = find_entry(s, delegateId, NULL);
        if (i >= 0) out[k++] = s->items[i];
    }
    return k;
}

int att_session_tally(const char *sessionId, int counts[ATT_STATUS_COUNT])
{
    Sheet *s;
    int i;
    for (i = 0; i < ATT_STATUS_COUNT; i++) counts[i] = 0;
    if (!sessionId) return 0;
    s = find_sheet(g_sheets, sessionId);
    if (!s) return 0;
    for (i = 0; i < s->count; i++) counts[s->items[i].status]++;
    return s->count;
}

int att_save(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    Sheet *s;
    int i;
    FILE *f = pers_begin_save(path, "attendance", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (s = g_sheets; s; s = s->next) {
        for (i = 0; i < s->count; i++) {
            const AttendanceRec *a = &s->items[i];
            pers_write_field(f, a->sessionId);  fputc('|', f);
            pers_write_field(f, a->delegateId); fputc('|', f);
            pers_write_int(f, a->status);        fputc('|', f);
            pers_write_field(f, a->markedBy);   fputc('|', f);
            pers_write_field(f, a->markedAt);   fputc('\n', f);
        }
    }
    return pers_finish_save(f, tmp, path);
}

int att_load(const char *path, int *skipped)
{
    Sheet *head = NULL;
    int total = 0, bad = 0, r, rc;
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f;
    rc = pers_open_for_load(path, "attendance", &f);
    if (rc != DSA_OK) return rc;
    if (f) {
        while ((r = pers_read_line(f, line, sizeof line)) != 0) {
            AttendanceRec a;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&a, 0, sizeof a);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 5) { bad++; continue; }
            pers_copy_field(a.sessionId, fld[0], DSA_ID_LEN);
            pers_copy_field(a.delegateId, fld[1], DSA_ID_LEN);
            a.status = pers_parse_int(fld[2]);
            pers_copy_field(a.markedBy, fld[3], DSA_ID_LEN);
            pers_copy_field(a.markedAt, fld[4], DSA_STAMP_LEN);
            rc = mark_into(&head, &total, &a, NULL);
            if (rc == DSA_ERR_NO_MEMORY) { fclose(f); free_sheets(&head); return rc; }
            if (rc != DSA_OK) bad++;
        }
        fclose(f);
    }
    att_store_reset();
    g_sheets = head;
    g_total = total;
    if (skipped) *skipped += bad;
    return DSA_OK;
}
