/*
 * MUNSphere — Module 6: Motion Management (C)
 *
 * PRIORITY QUEUE — binary min-heap of pending motions, one per session.
 *   Key, compared in order:
 *     1. motion type: its precedence (MotionType enum order; adjournment first)
 *     2. total duration, LONGER first (the most disruptive caucus is taken first)
 *     3. arrival sequence, earlier first (keeps equal motions FIFO)
 *   insert and remove-top are O(log n): sift up / sift down.
 *   Removing a motion from the middle (withdrawn, ruled out of order)
 *   swaps it with the last element and re-sifts, also O(log n).
 *
 * UNDO STACK — linked-list stack of chair actions, one per session.
 *   push O(1), pop O(1): undo always reverses the MOST RECENT action.
 *
 * All motions are also kept in a linked list (the permanent record).
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

typedef struct MotionNode { MotionRec data; struct MotionNode *next; } MotionNode;

typedef struct {
    char id[DSA_ID_LEN];
    int type, minutes;
    long seq;
} HeapItem;

typedef struct ActionNode { MotionActionRec data; struct ActionNode *next; } ActionNode;

typedef struct SessionMotions {
    char sessionId[DSA_ID_LEN];
    HeapItem *heap;
    int size, capacity;
    ActionNode *undo;               /* stack top */
    int depth;
    struct SessionMotions *next;
} SessionMotions;

static MotionNode *g_motions;      /* newest first */
static SessionMotions *g_sessions;
static long g_seq;

/* ------------------------------------------------------------ helpers */

static SessionMotions *sm_get(const char *sessionId, int create)
{
    SessionMotions *s;
    for (s = g_sessions; s; s = s->next)
        if (strcmp(s->sessionId, sessionId) == 0) return s;
    if (!create) return NULL;
    s = (SessionMotions *)calloc(1, sizeof *s);
    if (!s) return NULL;
    pers_copy_field(s->sessionId, sessionId, DSA_ID_LEN);
    s->next = g_sessions;
    g_sessions = s;
    return s;
}

static MotionNode *motion_node(const char *id)
{
    MotionNode *n;
    for (n = g_motions; n; n = n->next)
        if (strcmp(n->data.id, id) == 0) return n;
    return NULL;
}

/* < 0 if a should be taken before b */
static int heap_cmp(const HeapItem *a, const HeapItem *b)
{
    if (a->type != b->type) return a->type - b->type;
    if (a->minutes != b->minutes) return b->minutes - a->minutes;   /* longer first */
    return a->seq < b->seq ? -1 : (a->seq > b->seq ? 1 : 0);
}

static void heap_swap(HeapItem *h, int i, int j) { HeapItem t = h[i]; h[i] = h[j]; h[j] = t; }

static void sift_up(HeapItem *h, int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap_cmp(&h[i], &h[parent]) >= 0) break;
        heap_swap(h, i, parent);
        i = parent;
    }
}

static void sift_down(HeapItem *h, int n, int i)
{
    for (;;) {
        int l = 2 * i + 1, r = l + 1, best = i;
        if (l < n && heap_cmp(&h[l], &h[best]) < 0) best = l;
        if (r < n && heap_cmp(&h[r], &h[best]) < 0) best = r;
        if (best == i) return;
        heap_swap(h, i, best);
        i = best;
    }
}

static int heap_push(SessionMotions *s, const MotionRec *m)
{
    if (s->size == s->capacity) {
        int cap = s->capacity ? s->capacity * 2 : 8;
        HeapItem *bigger = (HeapItem *)realloc(s->heap, (size_t)cap * sizeof *bigger);
        if (!bigger) return DSA_ERR_NO_MEMORY;
        s->heap = bigger;
        s->capacity = cap;
    }
    pers_copy_field(s->heap[s->size].id, m->id, DSA_ID_LEN);
    s->heap[s->size].type = m->type;
    s->heap[s->size].minutes = m->totalMinutes;
    s->heap[s->size].seq = m->seq;
    sift_up(s->heap, s->size);
    s->size++;
    return DSA_OK;
}

static void heap_remove_id(SessionMotions *s, const char *id)
{
    int i;
    for (i = 0; i < s->size; i++) {
        if (strcmp(s->heap[i].id, id) != 0) continue;
        s->size--;
        if (i != s->size) {
            s->heap[i] = s->heap[s->size];       /* move last into the hole */
            sift_up(s->heap, i);
            sift_down(s->heap, s->size, i);
        }
        return;
    }
}

/* ------------------------------------------------------------- public */

void motion_store_reset(void)
{
    MotionNode *m = g_motions, *mn;
    SessionMotions *s = g_sessions, *sn;
    while (m) { mn = m->next; free(m); m = mn; }
    while (s) {
        ActionNode *a = s->undo, *an;
        while (a) { an = a->next; free(a); a = an; }
        sn = s->next;
        free(s->heap);
        free(s);
        s = sn;
    }
    g_motions = NULL;
    g_sessions = NULL;
    g_seq = 0;
}

long motion_next_seq(void) { return ++g_seq; }

int motion_insert(const MotionRec *rec)
{
    MotionNode *n;
    SessionMotions *s;
    if (!rec || !rec->id[0] || !rec->sessionId[0]) return DSA_ERR_INVALID;
    if (rec->type < 0 || rec->type >= MOTION_TYPE_COUNT) return DSA_ERR_INVALID;
    if (rec->status < 0 || rec->status >= MOTION_STATUS_COUNT) return DSA_ERR_INVALID;
    if (motion_node(rec->id)) return DSA_ERR_DUPLICATE;
    s = sm_get(rec->sessionId, 1);
    if (!s) return DSA_ERR_NO_MEMORY;
    n = (MotionNode *)calloc(1, sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    if (rec->status == MS_PENDING && heap_push(s, rec) != DSA_OK) { free(n); return DSA_ERR_NO_MEMORY; }
    n->next = g_motions;
    g_motions = n;
    if (rec->seq > g_seq) g_seq = rec->seq;
    return DSA_OK;
}

int motion_find(const char *id, MotionRec *out)
{
    MotionNode *n;
    if (!id) return DSA_ERR_INVALID;
    n = motion_node(id);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

int motion_update(const MotionRec *rec)
{
    MotionNode *n;
    SessionMotions *s;
    int was;
    if (!rec || rec->status < 0 || rec->status >= MOTION_STATUS_COUNT) return DSA_ERR_INVALID;
    n = motion_node(rec->id);
    if (!n) return DSA_ERR_NOT_FOUND;
    s = sm_get(n->data.sessionId, 1);
    if (!s) return DSA_ERR_NO_MEMORY;
    was = n->data.status;
    n->data = *rec;
    if (was == MS_PENDING && rec->status != MS_PENDING) heap_remove_id(s, rec->id);
    else if (was != MS_PENDING && rec->status == MS_PENDING) return heap_push(s, rec);
    return DSA_OK;
}

int motion_peek(const char *sessionId, MotionRec *out)
{
    SessionMotions *s;
    if (!sessionId) return DSA_ERR_INVALID;
    s = sm_get(sessionId, 0);
    if (!s || s->size == 0) return DSA_ERR_NOT_FOUND;
    return motion_find(s->heap[0].id, out);
}

int motion_heap_size(const char *sessionId)
{
    SessionMotions *s = sessionId ? sm_get(sessionId, 0) : NULL;
    return s ? s->size : 0;
}

/* Priority order without disturbing the heap: copy it, then pop the copy. */
int motion_pending(const char *sessionId, MotionRec *out, int max)
{
    SessionMotions *s;
    HeapItem *copy;
    int n, k = 0;
    if (!sessionId || !out) return 0;
    s = sm_get(sessionId, 0);
    if (!s || s->size == 0) return 0;
    n = s->size;
    copy = (HeapItem *)malloc((size_t)n * sizeof *copy);
    if (!copy) return 0;
    memcpy(copy, s->heap, (size_t)n * sizeof *copy);
    while (n > 0 && k < max) {
        motion_find(copy[0].id, &out[k++]);
        copy[0] = copy[--n];
        sift_down(copy, n, 0);
    }
    free(copy);
    return k;
}

int motion_list(const char *sessionId, MotionRec *out, int max)
{
    MotionNode *n;
    int k = 0;
    if (!out) return 0;
    for (n = g_motions; n && k < max; n = n->next)
        if (!sessionId || !*sessionId || strcmp(n->data.sessionId, sessionId) == 0) out[k++] = n->data;
    return k;
}

int motion_max_id_suffix(const char *prefix)
{
    MotionNode *n;
    int best = 0;
    size_t pl;
    if (!prefix) return 0;
    pl = strlen(prefix);
    for (n = g_motions; n; n = n->next) {
        const char *p = n->data.id;
        long v = 0;
        if (strncmp(p, prefix, pl) != 0 || !p[pl]) continue;
        for (p += pl; *p >= '0' && *p <= '9' && v < 1000000000L; p++) v = v * 10 + (*p - '0');
        if (!*p && v > best) best = (int)v;
    }
    return best;
}

/* ------------------------------------------------------- undo stack */

int mact_push(const MotionActionRec *a)
{
    SessionMotions *s;
    ActionNode *n;
    if (!a || !a->sessionId[0]) return DSA_ERR_INVALID;
    s = sm_get(a->sessionId, 1);
    if (!s) return DSA_ERR_NO_MEMORY;
    n = (ActionNode *)calloc(1, sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *a;
    n->next = s->undo;       /* push onto the top */
    s->undo = n;
    s->depth++;
    return DSA_OK;
}

int mact_peek(const char *sessionId, MotionActionRec *out)
{
    SessionMotions *s = sessionId ? sm_get(sessionId, 0) : NULL;
    if (!s || !s->undo) return DSA_ERR_NOT_FOUND;
    if (out) *out = s->undo->data;
    return DSA_OK;
}

int mact_pop(const char *sessionId, MotionActionRec *out)
{
    SessionMotions *s = sessionId ? sm_get(sessionId, 0) : NULL;
    ActionNode *top;
    if (!s || !s->undo) return DSA_ERR_NOT_FOUND;
    top = s->undo;
    if (out) *out = top->data;
    s->undo = top->next;     /* pop the top */
    free(top);
    s->depth--;
    return DSA_OK;
}

int mact_depth(const char *sessionId)
{
    SessionMotions *s = sessionId ? sm_get(sessionId, 0) : NULL;
    return s ? s->depth : 0;
}

/* ------------------------------------------------------- file handling */

int motions_save(const char *motionPath, const char *actionPath)
{
    char tmp[DSA_PATH_LEN + 8];
    MotionNode *n;
    SessionMotions *s;
    FILE *f;
    int rc;
    if (!(f = pers_begin_save(motionPath, "motions", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (n = g_motions; n; n = n->next) {
        const MotionRec *m = &n->data;
        pers_write_field(f, m->id); fputc('|', f);
        pers_write_field(f, m->sessionId); fputc('|', f);
        pers_write_field(f, m->committeeId); fputc('|', f);
        pers_write_field(f, m->proposerId); fputc('|', f);
        pers_write_int(f, m->type); fputc('|', f);
        pers_write_field(f, m->topic); fputc('|', f);
        pers_write_int(f, m->totalMinutes); fputc('|', f);
        pers_write_int(f, m->speakingSeconds); fputc('|', f);
        pers_write_int(f, m->status); fputc('|', f);
        pers_write_field(f, m->createdAt); fputc('|', f);
        fprintf(f, "%ld", m->seq); fputc('|', f);
        pers_write_field(f, m->voteId); fputc('\n', f);
    }
    if ((rc = pers_finish_save(f, tmp, motionPath)) != DSA_OK) return rc;

    /* Stacks are written bottom-to-top so loading can push in order. */
    if (!(f = pers_begin_save(actionPath, "motion_actions", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (s = g_sessions; s; s = s->next) {
        ActionNode *a;
        MotionActionRec *arr = (MotionActionRec *)malloc((size_t)(s->depth ? s->depth : 1) * sizeof *arr);
        int k = 0, i;
        if (!arr) { fclose(f); remove(tmp); return DSA_ERR_NO_MEMORY; }
        for (a = s->undo; a; a = a->next) arr[k++] = a->data;
        for (i = k - 1; i >= 0; i--) {
            pers_write_field(f, arr[i].sessionId); fputc('|', f);
            pers_write_field(f, arr[i].motionId); fputc('|', f);
            pers_write_int(f, arr[i].fromStatus); fputc('|', f);
            pers_write_int(f, arr[i].toStatus); fputc('|', f);
            pers_write_field(f, arr[i].by); fputc('|', f);
            pers_write_field(f, arr[i].at); fputc('\n', f);
        }
        free(arr);
    }
    return pers_finish_save(f, tmp, actionPath);
}

int motions_load(const char *motionPath, const char *actionPath, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f1, *f2;
    int bad = 0, r, rc;
    MotionRec *all = NULL;
    int n = 0, cap = 0, i;
    if ((rc = pers_open_for_load(motionPath, "motions", &f1)) != DSA_OK) return rc;
    if ((rc = pers_open_for_load(actionPath, "motion_actions", &f2)) != DSA_OK) { if (f1) fclose(f1); return rc; }
    motion_store_reset();
    if (f1) {
        while ((r = pers_read_line(f1, line, sizeof line)) != 0) {
            MotionRec m;
            char *end;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&m, 0, sizeof m);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 12) { bad++; continue; }
            pers_copy_field(m.id, fld[0], DSA_ID_LEN);
            pers_copy_field(m.sessionId, fld[1], DSA_ID_LEN);
            pers_copy_field(m.committeeId, fld[2], DSA_ID_LEN);
            pers_copy_field(m.proposerId, fld[3], DSA_ID_LEN);
            m.type = pers_parse_int(fld[4]);
            pers_copy_field(m.topic, fld[5], DSA_TEXT_LEN);
            m.totalMinutes = pers_parse_int(fld[6]);
            m.speakingSeconds = pers_parse_int(fld[7]);
            m.status = pers_parse_int(fld[8]);
            pers_copy_field(m.createdAt, fld[9], DSA_STAMP_LEN);
            m.seq = strtol(fld[10], &end, 10);
            pers_copy_field(m.voteId, fld[11], DSA_ID_LEN);
            if (n == cap) {
                MotionRec *bigger;
                cap = cap ? cap * 2 : 16;
                bigger = (MotionRec *)realloc(all, (size_t)cap * sizeof *bigger);
                if (!bigger) break;
                all = bigger;
            }
            all[n++] = m;
        }
        fclose(f1);
        for (i = n - 1; i >= 0; i--)          /* oldest first keeps list order */
            if (motion_insert(&all[i]) != DSA_OK) bad++;
        free(all);
    }
    if (f2) {
        while ((r = pers_read_line(f2, line, sizeof line)) != 0) {
            MotionActionRec a;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&a, 0, sizeof a);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 6) { bad++; continue; }
            pers_copy_field(a.sessionId, fld[0], DSA_ID_LEN);
            pers_copy_field(a.motionId, fld[1], DSA_ID_LEN);
            a.fromStatus = pers_parse_int(fld[2]);
            a.toStatus = pers_parse_int(fld[3]);
            pers_copy_field(a.by, fld[4], DSA_ID_LEN);
            pers_copy_field(a.at, fld[5], DSA_STAMP_LEN);
            if (mact_push(&a) != DSA_OK) bad++;
        }
        fclose(f2);
    }
    if (skipped) *skipped += bad;
    return DSA_OK;
}
