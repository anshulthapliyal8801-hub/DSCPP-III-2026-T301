/*
 * MUNSphere — Module 10: Executive Board assignments (C)
 *
 * Adjacency list (a bipartite graph: committees <-> EB members)
 *
 *   committee list:  [COM001] -> [COM002] -> NULL          (vertices)
 *                       |           |
 *                    [EB001]     [EB003]                    (edges)
 *                       |           |
 *                    [EB002]     [EB004]
 *
 * "Who runs UNGA?" walks one short list instead of every EB member.
 * Transferring an EB member unlinks the node from one committee's list and
 * links the same node into another: no copying, no reallocation.
 * A committee vertex is removed as soon as its member list becomes empty.
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

typedef struct EbNode {
    EbRec data;
    struct EbNode *next;
} EbNode;

typedef struct CommitteeVertex {
    char committeeId[DSA_ID_LEN];
    EbNode *members;
    int memberCount;
    struct CommitteeVertex *next;
} CommitteeVertex;

static CommitteeVertex *g_vertices;
static int g_ebCount;
static int g_vertexCount;

static void terminate_eb(EbRec *r)
{
    r->id[DSA_ID_LEN - 1] = '\0';
    r->committeeId[DSA_ID_LEN - 1] = '\0';
    r->email[DSA_NAME_LEN - 1] = '\0';
}

static CommitteeVertex *find_vertex(CommitteeVertex *head, const char *committeeId,
                                    CommitteeVertex **prevOut)
{
    CommitteeVertex *prev = NULL, *v = head;
    while (v && strcmp(v->committeeId, committeeId) != 0) { prev = v; v = v->next; }
    if (prevOut) *prevOut = prev;
    return v;
}

/* Searches every vertex's member list for an EB ID. */
static EbNode *find_member(CommitteeVertex *head, const char *id, CommitteeVertex **vOut,
                           EbNode **prevOut, int *cmpOut)
{
    CommitteeVertex *v;
    int cmp = 0;
    for (v = head; v; v = v->next) {
        EbNode *prev = NULL, *n;
        for (n = v->members; n; prev = n, n = n->next) {
            cmp++;
            if (strcmp(n->data.id, id) == 0) {
                if (vOut) *vOut = v;
                if (prevOut) *prevOut = prev;
                if (cmpOut) *cmpOut = cmp;
                return n;
            }
        }
    }
    if (cmpOut) *cmpOut = cmp;
    return NULL;
}

/* Links a node under its committee, creating the vertex if needed. */
static int link_node(CommitteeVertex **head, int *vertexCount, EbNode *n)
{
    CommitteeVertex *v = find_vertex(*head, n->data.committeeId, NULL);
    if (!v) {
        v = (CommitteeVertex *)calloc(1, sizeof *v);
        if (!v) return DSA_ERR_NO_MEMORY;
        pers_copy_field(v->committeeId, n->data.committeeId, DSA_ID_LEN);
        v->next = *head;
        *head = v;
        (*vertexCount)++;
    }
    n->next = v->members;
    v->members = n;
    v->memberCount++;
    return DSA_OK;
}

/* Unlinks a node; removes the vertex if it became empty. */
static void unlink_node(CommitteeVertex **head, int *vertexCount, CommitteeVertex *v,
                        EbNode *prev, EbNode *n)
{
    if (prev) prev->next = n->next; else v->members = n->next;
    n->next = NULL;
    v->memberCount--;
    if (v->memberCount == 0) {
        CommitteeVertex *vprev;
        find_vertex(*head, v->committeeId, &vprev);
        if (vprev) vprev->next = v->next; else *head = v->next;
        free(v);
        (*vertexCount)--;
    }
}

static int insert_into(CommitteeVertex **head, int *count, int *vcount, const EbRec *rec)
{
    EbNode *n;
    int rc;
    if (!rec || !rec->id[0] || !rec->committeeId[0]) return DSA_ERR_INVALID;
    if (rec->ebRole < 0 || rec->ebRole >= EB_ROLE_COUNT) return DSA_ERR_INVALID;
    if (find_member(*head, rec->id, NULL, NULL, NULL)) return DSA_ERR_DUPLICATE;
    n = (EbNode *)malloc(sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    terminate_eb(&n->data);
    rc = link_node(head, vcount, n);
    if (rc != DSA_OK) { free(n); return rc; }
    (*count)++;
    return DSA_OK;
}

static void free_graph(CommitteeVertex **head)
{
    CommitteeVertex *v = *head, *vnext;
    while (v) {
        EbNode *n = v->members, *nnext;
        while (n) { nnext = n->next; free(n); n = nnext; }
        vnext = v->next;
        free(v);
        v = vnext;
    }
    *head = NULL;
}

void eb_store_reset(void)
{
    free_graph(&g_vertices);
    g_ebCount = 0;
    g_vertexCount = 0;
}

int eb_insert(const EbRec *rec)
{
    return insert_into(&g_vertices, &g_ebCount, &g_vertexCount, rec);
}

int eb_find(const char *id, EbRec *out, int *comparisons)
{
    EbNode *n;
    if (!id) return DSA_ERR_INVALID;
    n = find_member(g_vertices, id, NULL, NULL, comparisons);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

int eb_update(const EbRec *rec)
{
    CommitteeVertex *v;
    EbNode *prev, *n;
    if (!rec || !rec->committeeId[0]) return DSA_ERR_INVALID;
    if (rec->ebRole < 0 || rec->ebRole >= EB_ROLE_COUNT) return DSA_ERR_INVALID;
    n = find_member(g_vertices, rec->id, &v, &prev, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (strcmp(n->data.committeeId, rec->committeeId) == 0) {
        n->data = *rec;
        terminate_eb(&n->data);
        return DSA_OK;
    }
    /* Transfer: move the same node to the other committee's list */
    unlink_node(&g_vertices, &g_vertexCount, v, prev, n);
    n->data = *rec;
    terminate_eb(&n->data);
    if (link_node(&g_vertices, &g_vertexCount, n) != DSA_OK) {
        free(n);
        g_ebCount--;
        return DSA_ERR_NO_MEMORY;
    }
    return DSA_OK;
}

int eb_delete(const char *id)
{
    CommitteeVertex *v;
    EbNode *prev, *n;
    if (!id) return DSA_ERR_INVALID;
    n = find_member(g_vertices, id, &v, &prev, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    unlink_node(&g_vertices, &g_vertexCount, v, prev, n);
    free(n);
    g_ebCount--;
    return DSA_OK;
}

int eb_count(void) { return g_ebCount; }
int eb_committee_count(void) { return g_vertexCount; }

int eb_count_by_committee(const char *committeeId)
{
    CommitteeVertex *v;
    if (!committeeId) return 0;
    v = find_vertex(g_vertices, committeeId, NULL);
    return v ? v->memberCount : 0;
}

int eb_list_by_committee(const char *committeeId, EbRec *out, int max)
{
    CommitteeVertex *v;
    EbNode *n;
    int k = 0;
    if (!committeeId || !out || max <= 0) return 0;
    v = find_vertex(g_vertices, committeeId, NULL);
    if (!v) return 0;
    for (n = v->members; n && k < max; n = n->next) out[k++] = n->data;
    return k;
}

int eb_to_array(EbRec *out, int max)
{
    CommitteeVertex *v;
    int k = 0;
    if (!out || max <= 0) return 0;
    for (v = g_vertices; v; v = v->next) {
        EbNode *n;
        for (n = v->members; n && k < max; n = n->next) out[k++] = n->data;
    }
    return k;
}

int eb_save(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    CommitteeVertex *v;
    FILE *f = pers_begin_save(path, "executive_board", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (v = g_vertices; v; v = v->next) {
        EbNode *n;
        for (n = v->members; n; n = n->next) {
            pers_write_field(f, n->data.id);          fputc('|', f);
            pers_write_field(f, n->data.committeeId); fputc('|', f);
            pers_write_int(f, n->data.ebRole);         fputc('|', f);
            pers_write_int(f, n->data.permissions);    fputc('|', f);
            pers_write_field(f, n->data.email);       fputc('\n', f);
        }
    }
    return pers_finish_save(f, tmp, path);
}

int eb_load(const char *path, int *skipped)
{
    CommitteeVertex *head = NULL;
    int count = 0, vcount = 0, bad = 0, r, rc, n = 0, cap = 0, i;
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    EbRec *all = NULL;
    FILE *f;
    rc = pers_open_for_load(path, "executive_board", &f);
    if (rc != DSA_OK) return rc;
    if (f) {
        while ((r = pers_read_line(f, line, sizeof line)) != 0) {
            EbRec e;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&e, 0, sizeof e);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 5) { bad++; continue; }
            pers_copy_field(e.id, fld[0], DSA_ID_LEN);
            pers_copy_field(e.committeeId, fld[1], DSA_ID_LEN);
            e.ebRole = pers_parse_int(fld[2]);
            e.permissions = pers_parse_int(fld[3]);
            pers_copy_field(e.email, fld[4], DSA_NAME_LEN);
            if (e.permissions < 0) { bad++; continue; }
            if (n == cap) {
                EbRec *bigger;
                cap = cap ? cap * 2 : 16;
                bigger = (EbRec *)realloc(all, (size_t)cap * sizeof *bigger);
                if (!bigger) { fclose(f); free(all); return DSA_ERR_NO_MEMORY; }
                all = bigger;
            }
            all[n++] = e;
        }
        fclose(f);
    }
    /* Every insert goes to the HEAD of its list, so inserting in reverse
     * file order rebuilds both the committee list and each member list in
     * exactly the order they were saved: save -> load -> save is stable. */
    for (i = n - 1; i >= 0; i--) {
        rc = insert_into(&head, &count, &vcount, &all[i]);
        if (rc == DSA_ERR_NO_MEMORY) { free(all); free_graph(&head); return rc; }
        if (rc != DSA_OK) bad++;
    }
    free(all);
    eb_store_reset();
    g_vertices = head;
    g_ebCount = count;
    g_vertexCount = vcount;
    if (skipped) *skipped += bad;
    return DSA_OK;
}
