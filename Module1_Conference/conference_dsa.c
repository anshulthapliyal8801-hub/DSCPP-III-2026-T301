/*
 * MUNSphere — Module 1: Conference & Session Management (C data structures)
 *
 * DSA concepts demonstrated
 *   Structures     : ConferenceRec, CommitteeRec, SessionRec (see dsa_api.h)
 *   Linked lists   : three singly linked lists that are the single in-memory
 *                    store for conferences, committees and sessions
 *   Sorted insert  : the session list is kept ordered by (date, start time)
 *   Linear search  : find-by-ID and name search, with comparison counting
 *   Arrays         : list-to-array export; session status name table
 *   File handling  : load/save through the shared Module 12 helpers
 *                    (pipe-delimited text, escaping, atomic replacement)
 *
 * Ownership: every node is allocated here and freed here. The C++ layer only
 * ever receives copies of records, never pointers into the lists.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

/* ------------------------------------------------------------------ */
/* Node and list types                                                 */
/* ------------------------------------------------------------------ */
typedef struct ConferenceNode {
    ConferenceRec data;
    struct ConferenceNode *next;
} ConferenceNode;

typedef struct CommitteeNode {
    CommitteeRec data;
    struct CommitteeNode *next;
} CommitteeNode;

typedef struct SessionNode {
    SessionRec data;
    struct SessionNode *next;
} SessionNode;

/* head + tail gives O(1) append; count gives O(1) size */
typedef struct { ConferenceNode *head, *tail; int count; } ConferenceList;
typedef struct { CommitteeNode  *head, *tail; int count; } CommitteeList;
typedef struct { SessionNode    *head;        int count; } SessionList;

/* The one shared store for Module 1 (static = private to this file) */
static ConferenceList g_conferences;
static CommitteeList  g_committees;
static SessionList    g_sessions;

/* Array demonstration: status code -> display name lookup table */
static const char *const SESSION_STATUS_NAMES[SESSION_STATUS_COUNT] = {
    "Scheduled", "Active", "Completed"
};

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

/* Guarantee '\0' termination of every fixed-size field we store. */
static void terminate_conference(ConferenceRec *r)
{
    r->id[DSA_ID_LEN - 1] = '\0';
    r->name[DSA_NAME_LEN - 1] = '\0';
    r->venue[DSA_NAME_LEN - 1] = '\0';
    r->startDate[DSA_DATE_LEN - 1] = '\0';
    r->endDate[DSA_DATE_LEN - 1] = '\0';
    r->description[DSA_TEXT_LEN - 1] = '\0';
}

static void terminate_committee(CommitteeRec *r)
{
    r->id[DSA_ID_LEN - 1] = '\0';
    r->conferenceId[DSA_ID_LEN - 1] = '\0';
    r->name[DSA_NAME_LEN - 1] = '\0';
    r->abbreviation[DSA_SHORT_LEN - 1] = '\0';
    r->agenda[DSA_TEXT_LEN - 1] = '\0';
}

static void terminate_session(SessionRec *r)
{
    r->id[DSA_ID_LEN - 1] = '\0';
    r->committeeId[DSA_ID_LEN - 1] = '\0';
    r->title[DSA_NAME_LEN - 1] = '\0';
    r->date[DSA_DATE_LEN - 1] = '\0';
    r->startTime[DSA_TIME_LEN - 1] = '\0';
    r->endTime[DSA_TIME_LEN - 1] = '\0';
}

/* Case-insensitive substring test used by the name search. */
static int contains_ci(const char *hay, const char *needle)
{
    size_t i, j, hn = strlen(hay), nn = strlen(needle);
    if (nn == 0) return 1;
    if (nn > hn) return 0;
    for (i = 0; i + nn <= hn; i++) {
        for (j = 0; j < nn; j++) {
            if (tolower((unsigned char)hay[i + j]) !=
                tolower((unsigned char)needle[j]))
                break;
        }
        if (j == nn) return 1;
    }
    return 0;
}

/* ================================================================== */
/* CONFERENCE LIST                                                     */
/* ================================================================== */

/* Linear search. Returns the node (or NULL), the previous node (needed
 * to unlink in a singly linked list) and the number of comparisons. */
static ConferenceNode *conf_list_find(const ConferenceList *L, const char *id,
                                      ConferenceNode **prevOut, int *cmpOut)
{
    ConferenceNode *prev = NULL, *cur = L->head;
    int cmp = 0;
    while (cur) {
        cmp++;
        if (strcmp(cur->data.id, id) == 0) break;
        prev = cur;
        cur = cur->next;
    }
    if (prevOut) *prevOut = prev;
    if (cmpOut) *cmpOut = cmp;
    return cur;
}

static int conf_list_append(ConferenceList *L, const ConferenceRec *rec)
{
    ConferenceNode *n;
    if (!rec || rec->id[0] == '\0') return DSA_ERR_INVALID;
    if (conf_list_find(L, rec->id, NULL, NULL)) return DSA_ERR_DUPLICATE;
    n = (ConferenceNode *)malloc(sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    terminate_conference(&n->data);
    n->next = NULL;
    if (L->tail) L->tail->next = n; else L->head = n;   /* O(1) append */
    L->tail = n;
    L->count++;
    return DSA_OK;
}

static void conf_list_free(ConferenceList *L)
{
    ConferenceNode *cur = L->head, *next;
    while (cur) { next = cur->next; free(cur); cur = next; }
    L->head = L->tail = NULL;
    L->count = 0;
}

int conference_insert(const ConferenceRec *rec)
{
    return conf_list_append(&g_conferences, rec);
}

int conference_find(const char *id, ConferenceRec *out, int *comparisons)
{
    ConferenceNode *n;
    if (!id) return DSA_ERR_INVALID;
    n = conf_list_find(&g_conferences, id, NULL, comparisons);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

int conference_update(const ConferenceRec *rec)
{
    ConferenceNode *n;
    if (!rec) return DSA_ERR_INVALID;
    n = conf_list_find(&g_conferences, rec->id, NULL, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    n->data = *rec;                 /* ID is the key and is unchanged */
    terminate_conference(&n->data);
    return DSA_OK;
}

int conference_delete(const char *id)
{
    ConferenceNode *prev, *n;
    if (!id) return DSA_ERR_INVALID;
    n = conf_list_find(&g_conferences, id, &prev, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (prev) prev->next = n->next; else g_conferences.head = n->next;
    if (g_conferences.tail == n) g_conferences.tail = prev;  /* removed tail */
    free(n);
    g_conferences.count--;
    return DSA_OK;
}

int conference_count(void) { return g_conferences.count; }

int conference_to_array(ConferenceRec *out, int max)
{
    int i = 0;
    ConferenceNode *cur;
    if (!out || max <= 0) return 0;
    for (cur = g_conferences.head; cur && i < max; cur = cur->next)
        out[i++] = cur->data;
    return i;
}

/* ================================================================== */
/* COMMITTEE LIST                                                      */
/* ================================================================== */

static CommitteeNode *com_list_find(const CommitteeList *L, const char *id,
                                    CommitteeNode **prevOut, int *cmpOut)
{
    CommitteeNode *prev = NULL, *cur = L->head;
    int cmp = 0;
    while (cur) {
        cmp++;
        if (strcmp(cur->data.id, id) == 0) break;
        prev = cur;
        cur = cur->next;
    }
    if (prevOut) *prevOut = prev;
    if (cmpOut) *cmpOut = cmp;
    return cur;
}

static int com_list_append(CommitteeList *L, const CommitteeRec *rec)
{
    CommitteeNode *n;
    if (!rec || rec->id[0] == '\0') return DSA_ERR_INVALID;
    if (com_list_find(L, rec->id, NULL, NULL)) return DSA_ERR_DUPLICATE;
    n = (CommitteeNode *)malloc(sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    terminate_committee(&n->data);
    n->next = NULL;
    if (L->tail) L->tail->next = n; else L->head = n;
    L->tail = n;
    L->count++;
    return DSA_OK;
}

static void com_list_free(CommitteeList *L)
{
    CommitteeNode *cur = L->head, *next;
    while (cur) { next = cur->next; free(cur); cur = next; }
    L->head = L->tail = NULL;
    L->count = 0;
}

int committee_insert(const CommitteeRec *rec)
{
    return com_list_append(&g_committees, rec);
}

int committee_find(const char *id, CommitteeRec *out, int *comparisons)
{
    CommitteeNode *n;
    if (!id) return DSA_ERR_INVALID;
    n = com_list_find(&g_committees, id, NULL, comparisons);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

int committee_update(const CommitteeRec *rec)
{
    CommitteeNode *n;
    if (!rec) return DSA_ERR_INVALID;
    n = com_list_find(&g_committees, rec->id, NULL, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    n->data = *rec;
    terminate_committee(&n->data);
    return DSA_OK;
}

int committee_delete(const char *id)
{
    CommitteeNode *prev, *n;
    if (!id) return DSA_ERR_INVALID;
    n = com_list_find(&g_committees, id, &prev, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (prev) prev->next = n->next; else g_committees.head = n->next;
    if (g_committees.tail == n) g_committees.tail = prev;
    free(n);
    g_committees.count--;
    return DSA_OK;
}

int committee_count(void) { return g_committees.count; }

int committee_to_array(CommitteeRec *out, int max)
{
    int i = 0;
    CommitteeNode *cur;
    if (!out || max <= 0) return 0;
    for (cur = g_committees.head; cur && i < max; cur = cur->next)
        out[i++] = cur->data;
    return i;
}

int committee_count_by_conference(const char *conferenceId)
{
    int c = 0;
    CommitteeNode *cur;
    if (!conferenceId) return 0;
    for (cur = g_committees.head; cur; cur = cur->next)
        if (strcmp(cur->data.conferenceId, conferenceId) == 0) c++;
    return c;
}

/* Linear search over every node: matches name or abbreviation,
 * case-insensitively. Each node examined counts as one comparison. */
int committee_search_name(const char *query, CommitteeRec *out, int max,
                          int *comparisons)
{
    int found = 0, cmp = 0;
    CommitteeNode *cur;
    if (!query) query = "";
    for (cur = g_committees.head; cur; cur = cur->next) {
        cmp++;
        if (contains_ci(cur->data.name, query) ||
            contains_ci(cur->data.abbreviation, query)) {
            if (out && found < max) out[found] = cur->data;
            found++;
        }
    }
    if (comparisons) *comparisons = cmp;
    return found < max ? found : max;
}

/* ================================================================== */
/* SESSION LIST — kept sorted by (date, startTime, id)                 */
/* ================================================================== */

static int session_key_cmp(const SessionRec *a, const SessionRec *b)
{
    int c = strcmp(a->date, b->date);         /* ISO dates sort as text */
    if (c) return c;
    c = strcmp(a->startTime, b->startTime);   /* HH:MM sorts as text    */
    if (c) return c;
    return strcmp(a->id, b->id);              /* tie-break: stable order */
}

/* Sorted insertion: walk until the next node's key is greater, then link.
 * O(n) per insert, but every listing is then already in time order. */
static void ses_list_link_sorted(SessionList *L, SessionNode *n)
{
    SessionNode *cur;
    if (!L->head || session_key_cmp(&n->data, &L->head->data) < 0) {
        n->next = L->head;
        L->head = n;
        return;
    }
    cur = L->head;
    while (cur->next && session_key_cmp(&cur->next->data, &n->data) <= 0)
        cur = cur->next;
    n->next = cur->next;
    cur->next = n;
}

static SessionNode *ses_list_find(const SessionList *L, const char *id,
                                  SessionNode **prevOut, int *cmpOut)
{
    SessionNode *prev = NULL, *cur = L->head;
    int cmp = 0;
    while (cur) {
        cmp++;
        if (strcmp(cur->data.id, id) == 0) break;
        prev = cur;
        cur = cur->next;
    }
    if (prevOut) *prevOut = prev;
    if (cmpOut) *cmpOut = cmp;
    return cur;
}

static int ses_list_insert(SessionList *L, const SessionRec *rec)
{
    SessionNode *n;
    if (!rec || rec->id[0] == '\0') return DSA_ERR_INVALID;
    if (rec->status < 0 || rec->status >= SESSION_STATUS_COUNT)
        return DSA_ERR_INVALID;
    if (ses_list_find(L, rec->id, NULL, NULL)) return DSA_ERR_DUPLICATE;
    n = (SessionNode *)malloc(sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    terminate_session(&n->data);
    n->next = NULL;
    ses_list_link_sorted(L, n);
    L->count++;
    return DSA_OK;
}

static void ses_list_free(SessionList *L)
{
    SessionNode *cur = L->head, *next;
    while (cur) { next = cur->next; free(cur); cur = next; }
    L->head = NULL;
    L->count = 0;
}

int session_insert(const SessionRec *rec)
{
    return ses_list_insert(&g_sessions, rec);
}

int session_find(const char *id, SessionRec *out, int *comparisons)
{
    SessionNode *n;
    if (!id) return DSA_ERR_INVALID;
    n = ses_list_find(&g_sessions, id, NULL, comparisons);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

/* The date/time may change, so the node is unlinked, updated and
 * re-linked at its new sorted position. No reallocation is needed. */
int session_update(const SessionRec *rec)
{
    SessionNode *prev, *n;
    if (!rec) return DSA_ERR_INVALID;
    if (rec->status < 0 || rec->status >= SESSION_STATUS_COUNT)
        return DSA_ERR_INVALID;
    n = ses_list_find(&g_sessions, rec->id, &prev, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (prev) prev->next = n->next; else g_sessions.head = n->next;
    n->data = *rec;
    terminate_session(&n->data);
    n->next = NULL;
    ses_list_link_sorted(&g_sessions, n);
    return DSA_OK;
}

int session_delete(const char *id)
{
    SessionNode *prev, *n;
    if (!id) return DSA_ERR_INVALID;
    n = ses_list_find(&g_sessions, id, &prev, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (prev) prev->next = n->next; else g_sessions.head = n->next;
    free(n);
    g_sessions.count--;
    return DSA_OK;
}

int session_count(void) { return g_sessions.count; }

int session_to_array(SessionRec *out, int max)
{
    int i = 0;
    SessionNode *cur;
    if (!out || max <= 0) return 0;
    for (cur = g_sessions.head; cur && i < max; cur = cur->next)
        out[i++] = cur->data;
    return i;
}

int session_count_by_committee(const char *committeeId)
{
    int c = 0;
    SessionNode *cur;
    if (!committeeId) return 0;
    for (cur = g_sessions.head; cur; cur = cur->next)
        if (strcmp(cur->data.committeeId, committeeId) == 0) c++;
    return c;
}

const char *session_status_name(int status)
{
    if (status < 0 || status >= SESSION_STATUS_COUNT) return "Unknown";
    return SESSION_STATUS_NAMES[status];
}

/* ================================================================== */
/* ID GENERATION SUPPORT                                               */
/* ================================================================== */

/* Returns N if id == prefix followed by digits only, else -1. */
static int id_suffix(const char *id, const char *prefix)
{
    size_t pl = strlen(prefix);
    const char *p;
    long v = 0;
    if (strncmp(id, prefix, pl) != 0) return -1;
    p = id + pl;
    if (*p == '\0') return -1;
    for (; *p; p++) {
        if (!isdigit((unsigned char)*p)) return -1;
        v = v * 10 + (*p - '0');
        if (v > 1000000000L) return -1;
    }
    return (int)v;
}

int m1_max_id_suffix(int entity, const char *prefix)
{
    int best = 0, v;
    if (!prefix) return 0;
    if (entity == M1_CONFERENCE) {
        ConferenceNode *c;
        for (c = g_conferences.head; c; c = c->next)
            if ((v = id_suffix(c->data.id, prefix)) > best) best = v;
    } else if (entity == M1_COMMITTEE) {
        CommitteeNode *c;
        for (c = g_committees.head; c; c = c->next)
            if ((v = id_suffix(c->data.id, prefix)) > best) best = v;
    } else if (entity == M1_SESSION) {
        SessionNode *s;
        for (s = g_sessions.head; s; s = s->next)
            if ((v = id_suffix(s->data.id, prefix)) > best) best = v;
    }
    return best;
}

void m1_store_reset(void)
{
    ses_list_free(&g_sessions);
    com_list_free(&g_committees);
    conf_list_free(&g_conferences);
}

/* ================================================================== */
/* FILE HANDLING — uses the shared Module 12 helpers                    */
/* (format, escaping and atomic replace: see persistence_dsa.c)        */
/* ================================================================== */

static int load_conferences(const char *path, ConferenceList *L, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f;
    int rc = pers_open_for_load(path, "conferences", &f), r;
    if (rc != DSA_OK || !f) return rc;
    while ((r = pers_read_line(f, line, sizeof line)) != 0) {
        ConferenceRec rec;
        if (r < 0) { (*skipped)++; continue; }
        if (line[0] == '\0') continue;
        memset(&rec, 0, sizeof rec);
        if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 6) { (*skipped)++; continue; }
        pers_copy_field(rec.id, fld[0], DSA_ID_LEN);
        pers_copy_field(rec.name, fld[1], DSA_NAME_LEN);
        pers_copy_field(rec.venue, fld[2], DSA_NAME_LEN);
        pers_copy_field(rec.startDate, fld[3], DSA_DATE_LEN);
        pers_copy_field(rec.endDate, fld[4], DSA_DATE_LEN);
        pers_copy_field(rec.description, fld[5], DSA_TEXT_LEN);
        rc = conf_list_append(L, &rec);
        if (rc == DSA_ERR_NO_MEMORY) { fclose(f); return rc; }
        if (rc != DSA_OK) (*skipped)++;
    }
    fclose(f);
    return DSA_OK;
}

static int load_committees(const char *path, CommitteeList *L, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f;
    int rc = pers_open_for_load(path, "committees", &f), r;
    if (rc != DSA_OK || !f) return rc;
    while ((r = pers_read_line(f, line, sizeof line)) != 0) {
        CommitteeRec rec;
        if (r < 0) { (*skipped)++; continue; }
        if (line[0] == '\0') continue;
        memset(&rec, 0, sizeof rec);
        if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 6) { (*skipped)++; continue; }
        pers_copy_field(rec.id, fld[0], DSA_ID_LEN);
        pers_copy_field(rec.conferenceId, fld[1], DSA_ID_LEN);
        pers_copy_field(rec.name, fld[2], DSA_NAME_LEN);
        pers_copy_field(rec.abbreviation, fld[3], DSA_SHORT_LEN);
        pers_copy_field(rec.agenda, fld[4], DSA_TEXT_LEN);
        rec.capacity = pers_parse_int(fld[5]);
        if (rec.capacity < 0) { (*skipped)++; continue; }
        rc = com_list_append(L, &rec);
        if (rc == DSA_ERR_NO_MEMORY) { fclose(f); return rc; }
        if (rc != DSA_OK) (*skipped)++;
    }
    fclose(f);
    return DSA_OK;
}

static int load_sessions(const char *path, SessionList *L, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f;
    int rc = pers_open_for_load(path, "sessions", &f), r;
    if (rc != DSA_OK || !f) return rc;
    while ((r = pers_read_line(f, line, sizeof line)) != 0) {
        SessionRec rec;
        if (r < 0) { (*skipped)++; continue; }
        if (line[0] == '\0') continue;
        memset(&rec, 0, sizeof rec);
        if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 7) { (*skipped)++; continue; }
        pers_copy_field(rec.id, fld[0], DSA_ID_LEN);
        pers_copy_field(rec.committeeId, fld[1], DSA_ID_LEN);
        pers_copy_field(rec.title, fld[2], DSA_NAME_LEN);
        pers_copy_field(rec.date, fld[3], DSA_DATE_LEN);
        pers_copy_field(rec.startTime, fld[4], DSA_TIME_LEN);
        pers_copy_field(rec.endTime, fld[5], DSA_TIME_LEN);
        rec.status = pers_parse_int(fld[6]);
        rc = ses_list_insert(L, &rec);           /* rejects bad status */
        if (rc == DSA_ERR_NO_MEMORY) { fclose(f); return rc; }
        if (rc != DSA_OK) (*skipped)++;
    }
    fclose(f);
    return DSA_OK;
}

/* Load into temporary lists first; only if all three files are valid
 * are the live lists replaced. A bad file never wipes current data. */
int m1_load(const char *conferencePath, const char *committeePath,
            const char *sessionPath, int *skippedLines)
{
    ConferenceList conf = {0};
    CommitteeList com = {0};
    SessionList ses = {0};
    int skipped = 0, rc;

    if (!conferencePath || !committeePath || !sessionPath)
        return DSA_ERR_INVALID;

    rc = load_conferences(conferencePath, &conf, &skipped);
    if (rc == DSA_OK) rc = load_committees(committeePath, &com, &skipped);
    if (rc == DSA_OK) rc = load_sessions(sessionPath, &ses, &skipped);

    if (rc != DSA_OK) {
        ses_list_free(&ses);
        com_list_free(&com);
        conf_list_free(&conf);
        return rc;
    }
    m1_store_reset();
    g_conferences = conf;
    g_committees = com;
    g_sessions = ses;
    if (skippedLines) *skippedLines = skipped;
    return DSA_OK;
}

static int save_conferences(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    ConferenceNode *c;
    FILE *f = pers_begin_save(path, "conferences", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (c = g_conferences.head; c; c = c->next) {
        pers_write_field(f, c->data.id);          fputc('|', f);
        pers_write_field(f, c->data.name);        fputc('|', f);
        pers_write_field(f, c->data.venue);       fputc('|', f);
        pers_write_field(f, c->data.startDate);   fputc('|', f);
        pers_write_field(f, c->data.endDate);     fputc('|', f);
        pers_write_field(f, c->data.description); fputc('\n', f);
    }
    return pers_finish_save(f, tmp, path);
}

static int save_committees(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    CommitteeNode *c;
    FILE *f = pers_begin_save(path, "committees", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (c = g_committees.head; c; c = c->next) {
        pers_write_field(f, c->data.id);           fputc('|', f);
        pers_write_field(f, c->data.conferenceId); fputc('|', f);
        pers_write_field(f, c->data.name);         fputc('|', f);
        pers_write_field(f, c->data.abbreviation); fputc('|', f);
        pers_write_field(f, c->data.agenda);       fputc('|', f);
        pers_write_int(f, c->data.capacity);       fputc('\n', f);
    }
    return pers_finish_save(f, tmp, path);
}

static int save_sessions(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    SessionNode *s;
    FILE *f = pers_begin_save(path, "sessions", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (s = g_sessions.head; s; s = s->next) {
        pers_write_field(f, s->data.id);          fputc('|', f);
        pers_write_field(f, s->data.committeeId); fputc('|', f);
        pers_write_field(f, s->data.title);       fputc('|', f);
        pers_write_field(f, s->data.date);        fputc('|', f);
        pers_write_field(f, s->data.startTime);   fputc('|', f);
        pers_write_field(f, s->data.endTime);     fputc('|', f);
        pers_write_int(f, s->data.status);        fputc('\n', f);
    }
    return pers_finish_save(f, tmp, path);
}

/* Each file is replaced atomically; a failure leaves that file as it was. */
int m1_save(const char *conferencePath, const char *committeePath,
            const char *sessionPath)
{
    int rc;
    if (!conferencePath || !committeePath || !sessionPath) return DSA_ERR_INVALID;
    rc = save_conferences(conferencePath);
    if (rc == DSA_OK) rc = save_committees(committeePath);
    if (rc == DSA_OK) rc = save_sessions(sessionPath);
    return rc;
}
