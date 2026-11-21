/*
 * MUNSphere — Module 7: Voting (C)
 *
 *   Linked list of votes (newest first). Each vote owns a dynamic ARRAY of
 *   ballots that doubles when full.
 *
 *   Duplicate-vote prevention: ballot_cast() linearly searches the vote's
 *   ballot array for the delegate first; a second ballot is refused.
 *   Authorized correction: ballot_correct() changes an existing ballot and
 *   counts the correction, so changed votes stay visible in the record.
 *   Counting: vote_tally() walks the array once, indexing a counts[] array
 *   by the choice (YES=0, NO=1, ABSTAIN=2). O(n).
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

typedef struct VoteNode {
    VoteRec data;
    BallotRec *ballots;
    int count, capacity;
    struct VoteNode *next;
} VoteNode;

static VoteNode *g_votes;

static VoteNode *vote_node(const char *id)
{
    VoteNode *n;
    for (n = g_votes; n; n = n->next)
        if (strcmp(n->data.id, id) == 0) return n;
    return NULL;
}

static int ballot_index(const VoteNode *v, const char *delegateId, int *cmpOut)
{
    int i, cmp = 0;
    for (i = 0; i < v->count; i++) {
        cmp++;
        if (strcmp(v->ballots[i].delegateId, delegateId) == 0) { if (cmpOut) *cmpOut = cmp; return i; }
    }
    if (cmpOut) *cmpOut = cmp;
    return -1;
}

void vote_store_reset(void)
{
    VoteNode *n = g_votes, *next;
    while (n) { next = n->next; free(n->ballots); free(n); n = next; }
    g_votes = NULL;
}

int vote_open(const VoteRec *rec)
{
    VoteNode *n;
    if (!rec || !rec->id[0] || !rec->sessionId[0]) return DSA_ERR_INVALID;
    if (rec->majority < MAJ_SIMPLE || rec->majority > MAJ_TWO_THIRDS) return DSA_ERR_INVALID;
    if (vote_node(rec->id)) return DSA_ERR_DUPLICATE;
    n = (VoteNode *)calloc(1, sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    n->next = g_votes;
    g_votes = n;
    return DSA_OK;
}

int vote_find(const char *id, VoteRec *out)
{
    VoteNode *n;
    if (!id) return DSA_ERR_INVALID;
    n = vote_node(id);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

int vote_update(const VoteRec *rec)
{
    VoteNode *n;
    if (!rec) return DSA_ERR_INVALID;
    n = vote_node(rec->id);
    if (!n) return DSA_ERR_NOT_FOUND;
    n->data = *rec;
    return DSA_OK;
}

int vote_open_for_session(const char *sessionId, VoteRec *out)
{
    VoteNode *n;
    if (!sessionId) return DSA_ERR_INVALID;
    for (n = g_votes; n; n = n->next)
        if (n->data.state == VS_OPEN && strcmp(n->data.sessionId, sessionId) == 0) {
            if (out) *out = n->data;
            return DSA_OK;
        }
    return DSA_ERR_NOT_FOUND;
}

int vote_list(const char *sessionId, VoteRec *out, int max)
{
    VoteNode *n;
    int k = 0;
    if (!out) return 0;
    for (n = g_votes; n && k < max; n = n->next)
        if (!sessionId || !*sessionId || strcmp(n->data.sessionId, sessionId) == 0) out[k++] = n->data;
    return k;
}

int vote_delete(const char *id)
{
    VoteNode *prev = NULL, *n;
    if (!id) return DSA_ERR_INVALID;
    for (n = g_votes; n; prev = n, n = n->next) {
        if (strcmp(n->data.id, id) != 0) continue;
        if (prev) prev->next = n->next; else g_votes = n->next;
        free(n->ballots);
        free(n);
        return DSA_OK;
    }
    return DSA_ERR_NOT_FOUND;
}

int vote_max_id_suffix(const char *prefix)
{
    VoteNode *n;
    int best = 0;
    size_t pl;
    if (!prefix) return 0;
    pl = strlen(prefix);
    for (n = g_votes; n; n = n->next) {
        const char *p = n->data.id;
        long v = 0;
        if (strncmp(p, prefix, pl) != 0 || !p[pl]) continue;
        for (p += pl; *p >= '0' && *p <= '9' && v < 1000000000L; p++) v = v * 10 + (*p - '0');
        if (!*p && v > best) best = (int)v;
    }
    return best;
}

int ballot_cast(const BallotRec *b)
{
    VoteNode *v;
    if (!b || !b->delegateId[0] || b->choice < 0 || b->choice >= VOTE_CHOICE_COUNT) return DSA_ERR_INVALID;
    v = vote_node(b->voteId);
    if (!v) return DSA_ERR_NOT_FOUND;
    if (ballot_index(v, b->delegateId, NULL) >= 0) return DSA_ERR_DUPLICATE;
    if (v->count == v->capacity) {
        int cap = v->capacity ? v->capacity * 2 : 16;
        BallotRec *bigger = (BallotRec *)realloc(v->ballots, (size_t)cap * sizeof *bigger);
        if (!bigger) return DSA_ERR_NO_MEMORY;
        v->ballots = bigger;
        v->capacity = cap;
    }
    v->ballots[v->count++] = *b;
    return DSA_OK;
}

int ballot_correct(const char *voteId, const char *delegateId, int choice)
{
    VoteNode *v;
    int i;
    if (!voteId || !delegateId || choice < 0 || choice >= VOTE_CHOICE_COUNT) return DSA_ERR_INVALID;
    v = vote_node(voteId);
    if (!v) return DSA_ERR_NOT_FOUND;
    i = ballot_index(v, delegateId, NULL);
    if (i < 0) return DSA_ERR_NOT_FOUND;
    if (v->ballots[i].choice != choice) {
        v->ballots[i].choice = choice;
        v->ballots[i].corrections++;
    }
    return DSA_OK;
}

int ballot_find(const char *voteId, const char *delegateId, BallotRec *out, int *comparisons)
{
    VoteNode *v;
    int i;
    if (comparisons) *comparisons = 0;
    if (!voteId || !delegateId) return DSA_ERR_INVALID;
    v = vote_node(voteId);
    if (!v) return DSA_ERR_NOT_FOUND;
    i = ballot_index(v, delegateId, comparisons);
    if (i < 0) return DSA_ERR_NOT_FOUND;
    if (out) *out = v->ballots[i];
    return DSA_OK;
}

int ballot_list(const char *voteId, BallotRec *out, int max)
{
    VoteNode *v;
    int i, k = 0;
    if (!voteId || !out) return 0;
    v = vote_node(voteId);
    if (!v) return 0;
    for (i = 0; i < v->count && k < max; i++) out[k++] = v->ballots[i];
    return k;
}

int vote_tally(const char *voteId, int counts[VOTE_CHOICE_COUNT])
{
    VoteNode *v;
    int i;
    for (i = 0; i < VOTE_CHOICE_COUNT; i++) counts[i] = 0;
    if (!voteId) return 0;
    v = vote_node(voteId);
    if (!v) return 0;
    for (i = 0; i < v->count; i++) counts[v->ballots[i].choice]++;
    return v->count;
}

int votes_save(const char *votePath, const char *ballotPath)
{
    char tmp[DSA_PATH_LEN + 8];
    VoteNode *n;
    FILE *f;
    int rc, i;
    if (!(f = pers_begin_save(votePath, "votes", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (n = g_votes; n; n = n->next) {
        const VoteRec *v = &n->data;
        pers_write_field(f, v->id); fputc('|', f);
        pers_write_field(f, v->sessionId); fputc('|', f);
        pers_write_field(f, v->committeeId); fputc('|', f);
        pers_write_field(f, v->subject); fputc('|', f);
        pers_write_field(f, v->motionId); fputc('|', f);
        pers_write_int(f, v->substantive); fputc('|', f);
        pers_write_int(f, v->majority); fputc('|', f);
        pers_write_int(f, v->state); fputc('|', f);
        pers_write_int(f, v->yes); fputc('|', f);
        pers_write_int(f, v->no); fputc('|', f);
        pers_write_int(f, v->abstain); fputc('|', f);
        fprintf(f, "%d", v->outcome); fputc('|', f);
        pers_write_field(f, v->openedAt); fputc('|', f);
        pers_write_field(f, v->closedAt); fputc('\n', f);
    }
    if ((rc = pers_finish_save(f, tmp, votePath)) != DSA_OK) return rc;
    if (!(f = pers_begin_save(ballotPath, "ballots", tmp, sizeof tmp))) return DSA_ERR_IO;
    for (n = g_votes; n; n = n->next) {
        for (i = 0; i < n->count; i++) {
            const BallotRec *b = &n->ballots[i];
            pers_write_field(f, b->voteId); fputc('|', f);
            pers_write_field(f, b->delegateId); fputc('|', f);
            pers_write_int(f, b->choice); fputc('|', f);
            pers_write_int(f, b->corrections); fputc('|', f);
            pers_write_field(f, b->castAt); fputc('\n', f);
        }
    }
    return pers_finish_save(f, tmp, ballotPath);
}

int votes_load(const char *votePath, const char *ballotPath, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f1, *f2;
    int bad = 0, r, rc, n = 0, cap = 0, i;
    VoteRec *all = NULL;
    if ((rc = pers_open_for_load(votePath, "votes", &f1)) != DSA_OK) return rc;
    if ((rc = pers_open_for_load(ballotPath, "ballots", &f2)) != DSA_OK) { if (f1) fclose(f1); return rc; }
    vote_store_reset();
    if (f1) {
        while ((r = pers_read_line(f1, line, sizeof line)) != 0) {
            VoteRec v;
            char *end;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&v, 0, sizeof v);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 14) { bad++; continue; }
            pers_copy_field(v.id, fld[0], DSA_ID_LEN);
            pers_copy_field(v.sessionId, fld[1], DSA_ID_LEN);
            pers_copy_field(v.committeeId, fld[2], DSA_ID_LEN);
            pers_copy_field(v.subject, fld[3], DSA_TEXT_LEN);
            pers_copy_field(v.motionId, fld[4], DSA_ID_LEN);
            v.substantive = pers_parse_int(fld[5]);
            v.majority = pers_parse_int(fld[6]);
            v.state = pers_parse_int(fld[7]);
            v.yes = pers_parse_int(fld[8]);
            v.no = pers_parse_int(fld[9]);
            v.abstain = pers_parse_int(fld[10]);
            v.outcome = (int)strtol(fld[11], &end, 10);
            pers_copy_field(v.openedAt, fld[12], DSA_STAMP_LEN);
            pers_copy_field(v.closedAt, fld[13], DSA_STAMP_LEN);
            if (n == cap) {
                VoteRec *bigger;
                cap = cap ? cap * 2 : 16;
                bigger = (VoteRec *)realloc(all, (size_t)cap * sizeof *bigger);
                if (!bigger) break;
                all = bigger;
            }
            all[n++] = v;
        }
        fclose(f1);
        for (i = n - 1; i >= 0; i--)
            if (vote_open(&all[i]) != DSA_OK) bad++;
        free(all);
    }
    if (f2) {
        while ((r = pers_read_line(f2, line, sizeof line)) != 0) {
            BallotRec b;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&b, 0, sizeof b);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 5) { bad++; continue; }
            pers_copy_field(b.voteId, fld[0], DSA_ID_LEN);
            pers_copy_field(b.delegateId, fld[1], DSA_ID_LEN);
            b.choice = pers_parse_int(fld[2]);
            b.corrections = pers_parse_int(fld[3]);
            pers_copy_field(b.castAt, fld[4], DSA_STAMP_LEN);
            if (ballot_cast(&b) != DSA_OK) bad++;
        }
        fclose(f2);
    }
    if (skipped) *skipped += bad;
    return DSA_OK;
}
