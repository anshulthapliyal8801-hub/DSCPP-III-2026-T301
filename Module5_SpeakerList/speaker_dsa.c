/*
 * MUNSphere — Module 5: Live speaker list, speaking requests, POIs (C)
 *
 * GENERAL SPEAKERS LIST — circular queue, one per session
 *   A fixed array of GSL_CAPACITY slots with `head` and `count`.
 *     enqueue: slot (head + count) % CAP     dequeue: slot head, head = (head+1) % CAP
 *   Indices wrap around, so slots freed at the front are reused without
 *   shifting anything: both operations are O(1).
 *
 * SPEAKING REQUESTS — linked-list queue (head + tail pointers), FIFO.
 *   Delegates ask to be added; the EB approves or declines in arrival order.
 *
 * SPEECH HISTORY — singly linked list, newest at the head.
 *
 * POINTS OF INFORMATION — every POI lives in one linked list (the record);
 *   pending POIs are ALSO in a FIFO queue of pointers into that list, so the
 *   EB always sees the oldest undecided POI first.
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

#define DEFAULT_SPEAKING_SECONDS 90

/* ------------------------------------------------------------ types */
typedef struct Gsl {
    char sessionId[DSA_ID_LEN];
    char slots[GSL_CAPACITY][DSA_ID_LEN];
    int head, count;
    int speakingSeconds;
    char current[DSA_ID_LEN];
    long long startedAt;
    struct Gsl *next;
} Gsl;

typedef struct SpeechNode { SpeechRec data; struct SpeechNode *next; } SpeechNode;
typedef struct ReqNode { SpeakRequestRec data; struct ReqNode *next; } ReqNode;
typedef struct PoiNode { PoiRec data; struct PoiNode *next; } PoiNode;
typedef struct PendNode { PoiNode *poi; struct PendNode *next; } PendNode;

static Gsl *g_gsl;
static SpeechNode *g_speeches;
static int g_speechCount;
static ReqNode *g_reqHead, *g_reqTail;
static PoiNode *g_pois;              /* newest first */
static int g_poiCount;
static PendNode *g_pendHead, *g_pendTail;

/* ------------------------------------------------ circular queue (GSL) */

static Gsl *gsl_get(const char *sessionId, int create)
{
    Gsl *q;
    for (q = g_gsl; q; q = q->next)
        if (strcmp(q->sessionId, sessionId) == 0) return q;
    if (!create) return NULL;
    q = (Gsl *)calloc(1, sizeof *q);
    if (!q) return NULL;
    pers_copy_field(q->sessionId, sessionId, DSA_ID_LEN);
    q->speakingSeconds = DEFAULT_SPEAKING_SECONDS;
    q->next = g_gsl;
    g_gsl = q;
    return q;
}

static int gsl_index_of(const Gsl *q, const char *delegateId)
{
    int i;
    for (i = 0; i < q->count; i++)
        if (strcmp(q->slots[(q->head + i) % GSL_CAPACITY], delegateId) == 0) return i;
    return -1;
}

int spk_state(const char *sessionId, SpeakerState *out)
{
    Gsl *q;
    if (!sessionId || !out) return DSA_ERR_INVALID;
    memset(out, 0, sizeof *out);
    pers_copy_field(out->sessionId, sessionId, DSA_ID_LEN);
    q = gsl_get(sessionId, 0);
    if (!q) { out->speakingSeconds = DEFAULT_SPEAKING_SECONDS; return DSA_OK; }
    out->speakingSeconds = q->speakingSeconds;
    pers_copy_field(out->current, q->current, DSA_ID_LEN);
    out->startedAt = q->startedAt;
    out->queueLength = q->count;
    return DSA_OK;
}

int spk_set_time(const char *sessionId, int seconds)
{
    Gsl *q;
    if (!sessionId || seconds < 10 || seconds > 600) return DSA_ERR_INVALID;
    q = gsl_get(sessionId, 1);
    if (!q) return DSA_ERR_NO_MEMORY;
    q->speakingSeconds = seconds;
    return DSA_OK;
}

int spk_enqueue(const char *sessionId, const char *delegateId)
{
    Gsl *q;
    if (!sessionId || !delegateId || !delegateId[0]) return DSA_ERR_INVALID;
    q = gsl_get(sessionId, 1);
    if (!q) return DSA_ERR_NO_MEMORY;
    if (gsl_index_of(q, delegateId) >= 0 || strcmp(q->current, delegateId) == 0)
        return DSA_ERR_DUPLICATE;              /* already waiting or speaking */
    if (q->count == GSL_CAPACITY) return DSA_ERR_FULL;
    pers_copy_field(q->slots[(q->head + q->count) % GSL_CAPACITY], delegateId, DSA_ID_LEN);
    q->count++;
    return DSA_OK;
}

int spk_queue(const char *sessionId, char (*ids)[DSA_ID_LEN], int max)
{
    Gsl *q;
    int i, k = 0;
    if (!sessionId || !ids) return 0;
    q = gsl_get(sessionId, 0);
    if (!q) return 0;
    for (i = 0; i < q->count && k < max; i++)
        pers_copy_field(ids[k++], q->slots[(q->head + i) % GSL_CAPACITY], DSA_ID_LEN);
    return k;
}

int spk_position(const char *sessionId, const char *delegateId)
{
    Gsl *q;
    if (!sessionId || !delegateId) return -1;
    q = gsl_get(sessionId, 0);
    return q ? gsl_index_of(q, delegateId) : -1;
}

/* Removing from the middle: shift every later element one slot towards
 * the front (with wrap-around), then shrink. O(n). */
int spk_remove(const char *sessionId, const char *delegateId)
{
    Gsl *q;
    int i, pos;
    if (!sessionId || !delegateId) return DSA_ERR_INVALID;
    q = gsl_get(sessionId, 0);
    if (!q || (pos = gsl_index_of(q, delegateId)) < 0) return DSA_ERR_NOT_FOUND;
    for (i = pos; i < q->count - 1; i++)
        memcpy(q->slots[(q->head + i) % GSL_CAPACITY],
               q->slots[(q->head + i + 1) % GSL_CAPACITY], DSA_ID_LEN);
    q->count--;
    return DSA_OK;
}

static void log_speech(Gsl *q, long long now)
{
    SpeechNode *n;
    if (!q->current[0]) return;
    n = (SpeechNode *)calloc(1, sizeof *n);
    if (n) {
        pers_copy_field(n->data.sessionId, q->sessionId, DSA_ID_LEN);
        pers_copy_field(n->data.delegateId, q->current, DSA_ID_LEN);
        n->data.startedAt = q->startedAt;
        n->data.seconds = (int)(now > q->startedAt ? now - q->startedAt : 0);
        n->data.allotted = q->speakingSeconds;
        n->next = g_speeches;
        g_speeches = n;
        g_speechCount++;
    }
    q->current[0] = '\0';
    q->startedAt = 0;
}

int spk_finish(const char *sessionId, long long now)
{
    Gsl *q;
    if (!sessionId) return DSA_ERR_INVALID;
    q = gsl_get(sessionId, 0);
    if (!q || !q->current[0]) return DSA_ERR_NOT_FOUND;
    log_speech(q, now);
    return DSA_OK;
}

int spk_next(const char *sessionId, long long now, char *idOut)
{
    Gsl *q;
    if (!sessionId) return DSA_ERR_INVALID;
    q = gsl_get(sessionId, 1);
    if (!q) return DSA_ERR_NO_MEMORY;
    log_speech(q, now);                         /* the current speech ends */
    if (q->count == 0) return DSA_ERR_NOT_FOUND;
    pers_copy_field(q->current, q->slots[q->head], DSA_ID_LEN);   /* dequeue */
    q->head = (q->head + 1) % GSL_CAPACITY;
    q->count--;
    q->startedAt = now;
    if (idOut) pers_copy_field(idOut, q->current, DSA_ID_LEN);
    return DSA_OK;
}

int speech_list(const char *sessionId, const char *delegateId, SpeechRec *out, int max)
{
    SpeechNode *n;
    int k = 0;
    if (!out) return 0;
    for (n = g_speeches; n && k < max; n = n->next) {
        if (sessionId && *sessionId && strcmp(n->data.sessionId, sessionId) != 0) continue;
        if (delegateId && *delegateId && strcmp(n->data.delegateId, delegateId) != 0) continue;
        out[k++] = n->data;
    }
    return k;
}

int speech_count(void) { return g_speechCount; }

/* ------------------------------------------------- request queue (FIFO) */

int req_push(const SpeakRequestRec *rec)
{
    ReqNode *n;
    if (!rec || !rec->sessionId[0] || !rec->delegateId[0]) return DSA_ERR_INVALID;
    for (n = g_reqHead; n; n = n->next)
        if (!strcmp(n->data.sessionId, rec->sessionId) && !strcmp(n->data.delegateId, rec->delegateId))
            return DSA_ERR_DUPLICATE;
    n = (ReqNode *)calloc(1, sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    if (g_reqTail) g_reqTail->next = n; else g_reqHead = n;   /* enqueue at tail */
    g_reqTail = n;
    return DSA_OK;
}

static int req_unlink(const char *sessionId, const char *delegateId, SpeakRequestRec *out)
{
    ReqNode *prev = NULL, *n;
    for (n = g_reqHead; n; prev = n, n = n->next) {
        if (strcmp(n->data.sessionId, sessionId) != 0) continue;
        if (delegateId && strcmp(n->data.delegateId, delegateId) != 0) continue;
        if (prev) prev->next = n->next; else g_reqHead = n->next;
        if (g_reqTail == n) g_reqTail = prev;
        if (out) *out = n->data;
        free(n);
        return DSA_OK;
    }
    return DSA_ERR_NOT_FOUND;
}

/* Dequeue: the oldest request for this session */
int req_pop_front(const char *sessionId, SpeakRequestRec *out)
{
    if (!sessionId) return DSA_ERR_INVALID;
    return req_unlink(sessionId, NULL, out);
}

int req_remove(const char *sessionId, const char *delegateId)
{
    if (!sessionId || !delegateId) return DSA_ERR_INVALID;
    return req_unlink(sessionId, delegateId, NULL);
}

int req_list(const char *sessionId, SpeakRequestRec *out, int max)
{
    ReqNode *n;
    int k = 0;
    if (!sessionId || !out) return 0;
    for (n = g_reqHead; n && k < max; n = n->next)
        if (strcmp(n->data.sessionId, sessionId) == 0) out[k++] = n->data;
    return k;
}

/* --------------------------------------------------------------- POIs */

static void pend_enqueue(PoiNode *p)
{
    PendNode *n = (PendNode *)calloc(1, sizeof *n);
    if (!n) return;
    n->poi = p;
    if (g_pendTail) g_pendTail->next = n; else g_pendHead = n;
    g_pendTail = n;
}

static void pend_remove(PoiNode *p)
{
    PendNode *prev = NULL, *n;
    for (n = g_pendHead; n; prev = n, n = n->next) {
        if (n->poi != p) continue;
        if (prev) prev->next = n->next; else g_pendHead = n->next;
        if (g_pendTail == n) g_pendTail = prev;
        free(n);
        return;
    }
}

static PoiNode *poi_node(const char *id)
{
    PoiNode *n;
    for (n = g_pois; n; n = n->next)
        if (strcmp(n->data.id, id) == 0) return n;
    return NULL;
}

int poi_insert(const PoiRec *rec)
{
    PoiNode *n;
    if (!rec || !rec->id[0] || rec->status < 0 || rec->status >= POI_STATUS_COUNT) return DSA_ERR_INVALID;
    if (poi_node(rec->id)) return DSA_ERR_DUPLICATE;
    n = (PoiNode *)calloc(1, sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    n->next = g_pois;
    g_pois = n;
    g_poiCount++;
    if (rec->status == POI_PENDING) pend_enqueue(n);
    return DSA_OK;
}

int poi_find(const char *id, PoiRec *out)
{
    PoiNode *n;
    if (!id) return DSA_ERR_INVALID;
    n = poi_node(id);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

int poi_update(const PoiRec *rec)
{
    PoiNode *n;
    int wasPending;
    if (!rec || rec->status < 0 || rec->status >= POI_STATUS_COUNT) return DSA_ERR_INVALID;
    n = poi_node(rec->id);
    if (!n) return DSA_ERR_NOT_FOUND;
    wasPending = n->data.status == POI_PENDING;
    n->data = *rec;
    if (wasPending && rec->status != POI_PENDING) pend_remove(n);
    else if (!wasPending && rec->status == POI_PENDING) pend_enqueue(n);
    return DSA_OK;
}

int poi_pending(const char *sessionId, PoiRec *out, int max)
{
    PendNode *n;
    int k = 0;
    if (!out) return 0;
    for (n = g_pendHead; n && k < max; n = n->next)
        if (!sessionId || !*sessionId || strcmp(n->poi->data.sessionId, sessionId) == 0)
            out[k++] = n->poi->data;
    return k;
}

int poi_list(const char *sessionId, const char *delegateId, PoiRec *out, int max)
{
    PoiNode *n;
    int k = 0;
    if (!out) return 0;
    for (n = g_pois; n && k < max; n = n->next) {
        if (sessionId && *sessionId && strcmp(n->data.sessionId, sessionId) != 0) continue;
        if (delegateId && *delegateId && strcmp(n->data.delegateId, delegateId) != 0) continue;
        out[k++] = n->data;
    }
    return k;
}

int poi_count(void) { return g_poiCount; }

int poi_max_id_suffix(const char *prefix)
{
    PoiNode *n;
    int best = 0;
    size_t pl;
    if (!prefix) return 0;
    pl = strlen(prefix);
    for (n = g_pois; n; n = n->next) {
        const char *p = n->data.id;
        long v = 0;
        if (strncmp(p, prefix, pl) != 0 || !p[pl]) continue;
        for (p += pl; *p >= '0' && *p <= '9' && v < 1000000000L; p++) v = v * 10 + (*p - '0');
        if (!*p && v > best) best = (int)v;
    }
    return best;
}

/* ------------------------------------------------------- reset / files */

void spk_store_reset(void)
{
    Gsl *q = g_gsl, *qn;
    SpeechNode *s = g_speeches, *sn;
    ReqNode *r = g_reqHead, *rn;
    PoiNode *p = g_pois, *pn;
    PendNode *d = g_pendHead, *dn;
    while (q) { qn = q->next; free(q); q = qn; }
    while (s) { sn = s->next; free(s); s = sn; }
    while (r) { rn = r->next; free(r); r = rn; }
    while (p) { pn = p->next; free(p); p = pn; }
    while (d) { dn = d->next; free(d); d = dn; }
    g_gsl = NULL; g_speeches = NULL; g_speechCount = 0;
    g_reqHead = g_reqTail = NULL;
    g_pois = NULL; g_poiCount = 0;
    g_pendHead = g_pendTail = NULL;
}

static void write_ll(FILE *f, long long v) { fprintf(f, "%lld", v); }

int speakers_save(const char *statePath, const char *speechPath, const char *requestPath,
                  const char *poiPath)
{
    char tmp[DSA_PATH_LEN + 8];
    FILE *f;
    int rc, i;
    Gsl *q;
    SpeechNode *s;
    ReqNode *r;
    PoiNode *p;

    /* GSL: sessionId|seconds|current|startedAt|queued ids separated by ',' */
    if (!(f = pers_begin_save(statePath, "speaker_lists", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (q = g_gsl; q; q = q->next) {
        pers_write_field(f, q->sessionId); fputc('|', f);
        pers_write_int(f, q->speakingSeconds); fputc('|', f);
        pers_write_field(f, q->current); fputc('|', f);
        write_ll(f, q->startedAt); fputc('|', f);
        for (i = 0; i < q->count; i++) {
            if (i) fputc(',', f);
            pers_write_field(f, q->slots[(q->head + i) % GSL_CAPACITY]);
        }
        fputc('\n', f);
    }
    if ((rc = pers_finish_save(f, tmp, statePath)) != DSA_OK) return rc;

    if (!(f = pers_begin_save(speechPath, "speeches", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (s = g_speeches; s; s = s->next) {      /* newest first */
        pers_write_field(f, s->data.sessionId); fputc('|', f);
        pers_write_field(f, s->data.delegateId); fputc('|', f);
        write_ll(f, s->data.startedAt); fputc('|', f);
        pers_write_int(f, s->data.seconds); fputc('|', f);
        pers_write_int(f, s->data.allotted); fputc('\n', f);
    }
    if ((rc = pers_finish_save(f, tmp, speechPath)) != DSA_OK) return rc;

    if (!(f = pers_begin_save(requestPath, "speak_requests", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (r = g_reqHead; r; r = r->next) {       /* queue order */
        pers_write_field(f, r->data.sessionId); fputc('|', f);
        pers_write_field(f, r->data.delegateId); fputc('|', f);
        pers_write_field(f, r->data.at); fputc('\n', f);
    }
    if ((rc = pers_finish_save(f, tmp, requestPath)) != DSA_OK) return rc;

    if (!(f = pers_begin_save(poiPath, "pois", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (p = g_pois; p; p = p->next) {          /* newest first */
        const PoiRec *d = &p->data;
        pers_write_field(f, d->id); fputc('|', f);
        pers_write_field(f, d->sessionId); fputc('|', f);
        pers_write_field(f, d->committeeId); fputc('|', f);
        pers_write_field(f, d->delegateId); fputc('|', f);
        pers_write_field(f, d->targetId); fputc('|', f);
        pers_write_field(f, d->text); fputc('|', f);
        pers_write_field(f, d->createdAt); fputc('|', f);
        pers_write_int(f, d->status); fputc('|', f);
        pers_write_field(f, d->decidedBy); fputc('|', f);
        pers_write_field(f, d->reason); fputc('\n', f);
    }
    return pers_finish_save(f, tmp, poiPath);
}

static long long parse_ll(const char *s)
{
    char *end;
    long long v;
    if (!s || !*s) return -1;
    v = strtoll(s, &end, 10);
    return (*end || v < 0) ? -1 : v;
}

int speakers_load(const char *statePath, const char *speechPath, const char *requestPath,
                  const char *poiPath, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f1, *f2, *f3, *f4;
    int bad = 0, r, rc;

    /* Check all four headers before touching the live store. */
    if ((rc = pers_open_for_load(statePath, "speaker_lists", &f1)) != DSA_OK) return rc;
    if ((rc = pers_open_for_load(speechPath, "speeches", &f2)) != DSA_OK) { if (f1) fclose(f1); return rc; }
    if ((rc = pers_open_for_load(requestPath, "speak_requests", &f3)) != DSA_OK) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return rc;
    }
    if ((rc = pers_open_for_load(poiPath, "pois", &f4)) != DSA_OK) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        if (f3) fclose(f3);
        return rc;
    }
    spk_store_reset();

    if (f1) {
        while ((r = pers_read_line(f1, line, sizeof line)) != 0) {
            Gsl *q;
            char *ids, *tok;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 5) { bad++; continue; }
            q = gsl_get(fld[0], 1);
            if (!q) break;
            q->speakingSeconds = pers_parse_int(fld[1]) >= 10 ? pers_parse_int(fld[1]) : DEFAULT_SPEAKING_SECONDS;
            pers_copy_field(q->current, fld[2], DSA_ID_LEN);
            q->startedAt = parse_ll(fld[3]) > 0 ? parse_ll(fld[3]) : 0;
            ids = fld[4];
            for (tok = strtok(ids, ","); tok; tok = strtok(NULL, ","))
                if (spk_enqueue(q->sessionId, tok) != DSA_OK) bad++;
        }
        fclose(f1);
    }
    if (f2) {
        SpeechNode *tail = NULL;
        while ((r = pers_read_line(f2, line, sizeof line)) != 0) {
            SpeechNode *n;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 5) { bad++; continue; }
            n = (SpeechNode *)calloc(1, sizeof *n);
            if (!n) break;
            pers_copy_field(n->data.sessionId, fld[0], DSA_ID_LEN);
            pers_copy_field(n->data.delegateId, fld[1], DSA_ID_LEN);
            n->data.startedAt = parse_ll(fld[2]);
            n->data.seconds = pers_parse_int(fld[3]);
            n->data.allotted = pers_parse_int(fld[4]);
            if (tail) tail->next = n; else g_speeches = n;   /* keep newest-first order */
            tail = n;
            g_speechCount++;
        }
        fclose(f2);
    }
    if (f3) {
        while ((r = pers_read_line(f3, line, sizeof line)) != 0) {
            SpeakRequestRec q;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&q, 0, sizeof q);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 3) { bad++; continue; }
            pers_copy_field(q.sessionId, fld[0], DSA_ID_LEN);
            pers_copy_field(q.delegateId, fld[1], DSA_ID_LEN);
            pers_copy_field(q.at, fld[2], DSA_STAMP_LEN);
            if (req_push(&q) != DSA_OK) bad++;
        }
        fclose(f3);
    }
    if (f4) {
        /* File is newest first; inserting each at the head would reverse
         * it, so collect then insert from oldest to newest. */
        PoiRec *all = NULL;
        int n = 0, cap = 0, i;
        while ((r = pers_read_line(f4, line, sizeof line)) != 0) {
            PoiRec p;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&p, 0, sizeof p);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 10) { bad++; continue; }
            pers_copy_field(p.id, fld[0], DSA_ID_LEN);
            pers_copy_field(p.sessionId, fld[1], DSA_ID_LEN);
            pers_copy_field(p.committeeId, fld[2], DSA_ID_LEN);
            pers_copy_field(p.delegateId, fld[3], DSA_ID_LEN);
            pers_copy_field(p.targetId, fld[4], DSA_ID_LEN);
            pers_copy_field(p.text, fld[5], DSA_TEXT_LEN);
            pers_copy_field(p.createdAt, fld[6], DSA_STAMP_LEN);
            p.status = pers_parse_int(fld[7]);
            pers_copy_field(p.decidedBy, fld[8], DSA_ID_LEN);
            pers_copy_field(p.reason, fld[9], DSA_TEXT_LEN);
            if (n == cap) {
                PoiRec *bigger;
                cap = cap ? cap * 2 : 16;
                bigger = (PoiRec *)realloc(all, (size_t)cap * sizeof *bigger);
                if (!bigger) break;
                all = bigger;
            }
            all[n++] = p;
        }
        fclose(f4);
        for (i = n - 1; i >= 0; i--)
            if (poi_insert(&all[i]) != DSA_OK) bad++;
        free(all);
    }
    if (skipped) *skipped += bad;
    return DSA_OK;
}
