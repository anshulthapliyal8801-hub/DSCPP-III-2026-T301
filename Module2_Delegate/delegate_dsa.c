/*
 * MUNSphere — Module 2: Delegate Registration & Management (C)
 *
 * PRIMARY STORE — hash table with separate chaining
 *   index = djb2(id) mod bucketCount. Starts with 8 buckets; when the
 *   load factor (records / buckets) would pass 0.75 the table doubles and
 *   every node is relinked into its new bucket (rehashing). Average lookup
 *   by Delegate ID is O(1).
 *
 * SECONDARY INDEX — binary search tree ordered by (name, id)
 *   Gives the alphabetical roster with an in-order traversal, name lookup
 *   in O(h), min/max, height and node count. Each BST node stores only
 *   the key (name, id); the record itself lives once, in the hash table.
 *
 * SEARCHES compared by the analytics page (all count real comparisons)
 *   hash lookup, BST name search, linear search, binary search.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

#define INITIAL_BUCKETS 8
#define MAX_LOAD_NUM 3        /* load factor limit 3/4 = 0.75, kept in integers */
#define MAX_LOAD_DEN 4

typedef struct DelegateNode {
    DelegateRec data;
    struct DelegateNode *next;
} DelegateNode;

typedef struct BstNode {
    char name[DSA_NAME_LEN];
    char id[DSA_ID_LEN];
    struct BstNode *left, *right;
} BstNode;

static DelegateNode **g_table;
static int g_buckets;
static int g_count;
static int g_rehashes;
static BstNode *g_root;

/* ------------------------------------------------------------ helpers */

static int ci_cmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || !ca) return ca - cb;
    }
}

static int ci_contains(const char *hay, const char *needle)
{
    size_t i, j, hn = strlen(hay), nn = strlen(needle);
    if (!nn) return 1;
    for (i = 0; i + nn <= hn; i++) {
        for (j = 0; j < nn; j++)
            if (tolower((unsigned char)hay[i + j]) != tolower((unsigned char)needle[j])) break;
        if (j == nn) return 1;
    }
    return 0;
}

static void trace_reset(SearchTrace *t)
{
    if (t) { t->comparisons = 0; t->pathLen = 0; }
}

static void trace_visit(SearchTrace *t, const char *id)
{
    if (!t) return;
    t->comparisons++;
    if (t->pathLen < 64) pers_copy_field(t->path[t->pathLen++], id, DSA_ID_LEN);
}

static void terminate_delegate(DelegateRec *r)
{
    int k;
    r->id[DSA_ID_LEN - 1] = '\0';
    r->name[DSA_NAME_LEN - 1] = '\0';
    r->email[DSA_NAME_LEN - 1] = '\0';
    r->institution[DSA_NAME_LEN - 1] = '\0';
    r->conferenceId[DSA_ID_LEN - 1] = '\0';
    for (k = 0; k < DELEGATE_PREFS; k++) r->prefs[k][DSA_ID_LEN - 1] = '\0';
    r->note[DSA_TEXT_LEN - 1] = '\0';
    r->committeeId[DSA_ID_LEN - 1] = '\0';
    r->portfolio[DSA_NAME_LEN - 1] = '\0';
    r->registeredAt[DSA_STAMP_LEN - 1] = '\0';
}

/* ========================================================= HASH TABLE */

/* djb2 by Dan Bernstein: h = h * 33 + c, starting from 5381 */
unsigned delegate_hash_value(const char *id)
{
    unsigned h = 5381u;
    if (!id) return h;
    while (*id) h = h * 33u + (unsigned char)*id++;
    return h;
}

static int bucket_of(const char *id, int buckets)
{
    return (int)(delegate_hash_value(id) % (unsigned)buckets);
}

static int ensure_table(void)
{
    if (g_table) return DSA_OK;
    g_table = (DelegateNode **)calloc(INITIAL_BUCKETS, sizeof *g_table);
    if (!g_table) return DSA_ERR_NO_MEMORY;
    g_buckets = INITIAL_BUCKETS;
    return DSA_OK;
}

/* Doubles the bucket array and relinks every node. Nodes are moved,
 * not copied, so pointers to records stay valid. O(n). */
static int rehash(void)
{
    int newSize = g_buckets * 2, i;
    DelegateNode **fresh = (DelegateNode **)calloc((size_t)newSize, sizeof *fresh);
    if (!fresh) return DSA_ERR_NO_MEMORY;
    for (i = 0; i < g_buckets; i++) {
        DelegateNode *n = g_table[i], *next;
        while (n) {
            int b = bucket_of(n->data.id, newSize);
            next = n->next;
            n->next = fresh[b];
            fresh[b] = n;
            n = next;
        }
    }
    free(g_table);
    g_table = fresh;
    g_buckets = newSize;
    g_rehashes++;
    return DSA_OK;
}

static DelegateNode *hash_find(const char *id, DelegateNode **prevOut, SearchTrace *t)
{
    DelegateNode *prev = NULL, *n;
    if (!g_table) return NULL;
    for (n = g_table[bucket_of(id, g_buckets)]; n; prev = n, n = n->next) {
        trace_visit(t, n->data.id);
        if (strcmp(n->data.id, id) == 0) break;
    }
    if (prevOut) *prevOut = prev;
    return n;
}

/* ================================================================ BST */

static int bst_key_cmp(const char *name, const char *id, const BstNode *n)
{
    int c = ci_cmp(name, n->name);
    return c ? c : strcmp(id, n->id);
}

static int bst_insert(const char *name, const char *id)
{
    BstNode **p = &g_root, *n;
    while (*p) p = bst_key_cmp(name, id, *p) < 0 ? &(*p)->left : &(*p)->right;
    n = (BstNode *)calloc(1, sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    pers_copy_field(n->name, name, DSA_NAME_LEN);
    pers_copy_field(n->id, id, DSA_ID_LEN);
    *p = n;
    return DSA_OK;
}

/* Deletion, the three textbook cases:
 *   leaf or one child : replace the node by its only child (or NULL)
 *   two children      : copy the in-order successor's key into the node,
 *                       then remove the successor (it has no left child) */
static int bst_remove(const char *name, const char *id)
{
    BstNode **p = &g_root, *n;
    while (*p) {
        int c = bst_key_cmp(name, id, *p);
        if (c == 0) break;
        p = c < 0 ? &(*p)->left : &(*p)->right;
    }
    if (!*p) return DSA_ERR_NOT_FOUND;
    n = *p;
    if (!n->left) {
        *p = n->right;
        free(n);
    } else if (!n->right) {
        *p = n->left;
        free(n);
    } else {
        BstNode **s = &n->right, *succ;
        while ((*s)->left) s = &(*s)->left;
        succ = *s;
        *s = succ->right;
        memcpy(n->name, succ->name, sizeof n->name);
        memcpy(n->id, succ->id, sizeof n->id);
        free(succ);
    }
    return DSA_OK;
}

static void bst_free(BstNode *n)
{
    if (!n) return;
    bst_free(n->left);
    bst_free(n->right);
    free(n);
}

static void walk_in(const BstNode *n, char (*ids)[DSA_ID_LEN], int max, int *k)
{
    if (!n) return;
    walk_in(n->left, ids, max, k);
    if (*k < max) pers_copy_field(ids[(*k)++], n->id, DSA_ID_LEN);
    walk_in(n->right, ids, max, k);
}

static void walk_pre(const BstNode *n, char (*ids)[DSA_ID_LEN], int max, int *k)
{
    if (!n) return;
    if (*k < max) pers_copy_field(ids[(*k)++], n->id, DSA_ID_LEN);
    walk_pre(n->left, ids, max, k);
    walk_pre(n->right, ids, max, k);
}

static void walk_post(const BstNode *n, char (*ids)[DSA_ID_LEN], int max, int *k)
{
    if (!n) return;
    walk_post(n->left, ids, max, k);
    walk_post(n->right, ids, max, k);
    if (*k < max) pers_copy_field(ids[(*k)++], n->id, DSA_ID_LEN);
}

int bst_inorder(char (*ids)[DSA_ID_LEN], int max)  { int k = 0; if (ids) walk_in(g_root, ids, max, &k); return k; }
int bst_preorder(char (*ids)[DSA_ID_LEN], int max) { int k = 0; if (ids) walk_pre(g_root, ids, max, &k); return k; }
int bst_postorder(char (*ids)[DSA_ID_LEN], int max){ int k = 0; if (ids) walk_post(g_root, ids, max, &k); return k; }

static int height_of(const BstNode *n)
{
    int l, r;
    if (!n) return 0;
    l = height_of(n->left);
    r = height_of(n->right);
    return 1 + (l > r ? l : r);
}

static int count_of(const BstNode *n)
{
    return n ? 1 + count_of(n->left) + count_of(n->right) : 0;
}

int bst_height(void) { return height_of(g_root); }
int bst_node_count(void) { return count_of(g_root); }

int bst_min(char *idOut)
{
    const BstNode *n = g_root;
    if (!n) return DSA_ERR_NOT_FOUND;
    while (n->left) n = n->left;           /* leftmost node */
    if (idOut) pers_copy_field(idOut, n->id, DSA_ID_LEN);
    return DSA_OK;
}

int bst_max(char *idOut)
{
    const BstNode *n = g_root;
    if (!n) return DSA_ERR_NOT_FOUND;
    while (n->right) n = n->right;         /* rightmost node */
    if (idOut) pers_copy_field(idOut, n->id, DSA_ID_LEN);
    return DSA_OK;
}

/* Breadth-first walk using an array as a queue. */
int bst_level_order(char (*ids)[DSA_ID_LEN], int *parent, int *isLeft, int max)
{
    const BstNode **queue;
    int head = 0, tail = 0;
    if (!g_root || !ids || max <= 0) return 0;
    queue = (const BstNode **)malloc((size_t)max * sizeof *queue);
    if (!queue) return 0;
    queue[tail] = g_root;
    if (parent) parent[tail] = -1;
    if (isLeft) isLeft[tail] = 0;
    tail++;
    while (head < tail) {
        const BstNode *n = queue[head];
        pers_copy_field(ids[head], n->id, DSA_ID_LEN);
        if (n->left && tail < max) {
            queue[tail] = n->left;
            if (parent) parent[tail] = head;
            if (isLeft) isLeft[tail] = 1;
            tail++;
        }
        if (n->right && tail < max) {
            queue[tail] = n->right;
            if (parent) parent[tail] = head;
            if (isLeft) isLeft[tail] = 0;
            tail++;
        }
        head++;
    }
    free(queue);
    return tail;
}

/* ===================================================== PUBLIC STORE API */

void delegate_store_reset(void)
{
    int i;
    for (i = 0; i < g_buckets; i++) {
        DelegateNode *n = g_table[i], *next;
        while (n) { next = n->next; free(n); n = next; }
    }
    free(g_table);
    g_table = NULL;
    g_buckets = 0;
    g_count = 0;
    g_rehashes = 0;
    bst_free(g_root);
    g_root = NULL;
}

int delegate_insert(const DelegateRec *rec)
{
    DelegateNode *n;
    int b, rc;
    if (!rec || !rec->id[0] || !rec->name[0]) return DSA_ERR_INVALID;
    if (rec->status < 0 || rec->status >= REGISTRATION_STATUS_COUNT) return DSA_ERR_INVALID;
    if ((rc = ensure_table()) != DSA_OK) return rc;
    if (hash_find(rec->id, NULL, NULL)) return DSA_ERR_DUPLICATE;
    /* Grow before inserting if the load factor would pass 0.75 */
    if ((g_count + 1) * MAX_LOAD_DEN > g_buckets * MAX_LOAD_NUM) {
        if ((rc = rehash()) != DSA_OK) return rc;
    }
    n = (DelegateNode *)malloc(sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    terminate_delegate(&n->data);
    if ((rc = bst_insert(n->data.name, n->data.id)) != DSA_OK) { free(n); return rc; }
    b = bucket_of(n->data.id, g_buckets);
    n->next = g_table[b];
    g_table[b] = n;
    g_count++;
    return DSA_OK;
}

int delegate_find(const char *id, DelegateRec *out, int *comparisons)
{
    SearchTrace t;
    DelegateNode *n;
    if (!id) return DSA_ERR_INVALID;
    trace_reset(&t);
    n = hash_find(id, NULL, &t);
    if (comparisons) *comparisons = t.comparisons;
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

int delegate_update(const DelegateRec *rec)
{
    DelegateNode *n;
    DelegateRec copy;
    if (!rec || !rec->name[0]) return DSA_ERR_INVALID;
    if (rec->status < 0 || rec->status >= REGISTRATION_STATUS_COUNT) return DSA_ERR_INVALID;
    n = hash_find(rec->id, NULL, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    copy = *rec;
    terminate_delegate(&copy);
    if (strcmp(copy.name, n->data.name) != 0) {        /* BST key changed */
        if (bst_insert(copy.name, copy.id) != DSA_OK) return DSA_ERR_NO_MEMORY;
        bst_remove(n->data.name, n->data.id);
    }
    n->data = copy;
    return DSA_OK;
}

int delegate_delete(const char *id)
{
    DelegateNode *prev, *n;
    if (!id) return DSA_ERR_INVALID;
    n = hash_find(id, &prev, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (prev) prev->next = n->next;
    else g_table[bucket_of(id, g_buckets)] = n->next;
    bst_remove(n->data.name, n->data.id);
    free(n);
    g_count--;
    return DSA_OK;
}

int delegate_count(void) { return g_count; }

int delegate_to_array(DelegateRec *out, int max)
{
    int i, k = 0;
    if (!out || max <= 0) return 0;
    for (i = 0; i < g_buckets && k < max; i++) {
        DelegateNode *n;
        for (n = g_table[i]; n && k < max; n = n->next) out[k++] = n->data;
    }
    return k;
}

int delegate_max_id_suffix(const char *prefix)
{
    int i, best = 0;
    size_t pl;
    if (!prefix) return 0;
    pl = strlen(prefix);
    for (i = 0; i < g_buckets; i++) {
        DelegateNode *n;
        for (n = g_table[i]; n; n = n->next) {
            const char *p = n->data.id;
            long v = 0;
            if (strncmp(p, prefix, pl) != 0 || p[pl] == '\0') continue;
            for (p += pl; *p >= '0' && *p <= '9' && v < 1000000000L; p++) v = v * 10 + (*p - '0');
            if (*p == '\0' && v > best) best = (int)v;
        }
    }
    return best;
}

/* ---------------------------------------------- hash table inspection */

int delegate_bucket_count(void) { return g_buckets; }
int delegate_rehash_count(void) { return g_rehashes; }

int delegate_bucket_length(int bucket)
{
    DelegateNode *n;
    int c = 0;
    if (bucket < 0 || bucket >= g_buckets) return 0;
    for (n = g_table[bucket]; n; n = n->next) c++;
    return c;
}

int delegate_bucket_ids(int bucket, char (*ids)[DSA_ID_LEN], int max)
{
    DelegateNode *n;
    int k = 0;
    if (bucket < 0 || bucket >= g_buckets || !ids) return 0;
    for (n = g_table[bucket]; n && k < max; n = n->next)
        pers_copy_field(ids[k++], n->data.id, DSA_ID_LEN);
    return k;
}

double delegate_load_factor(void)
{
    return g_buckets ? (double)g_count / (double)g_buckets : 0.0;
}

/* ============================================================ SEARCHES */

int delegate_search_hash(const char *id, DelegateRec *out, SearchTrace *t)
{
    DelegateNode *n;
    trace_reset(t);
    if (!id) return DSA_ERR_INVALID;
    n = hash_find(id, NULL, t);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

/* Exact, case-insensitive name match; O(h) */
int delegate_search_bst_name(const char *name, DelegateRec *out, SearchTrace *t)
{
    const BstNode *n = g_root;
    trace_reset(t);
    if (!name) return DSA_ERR_INVALID;
    while (n) {
        int c;
        trace_visit(t, n->id);
        c = ci_cmp(name, n->name);
        if (c == 0) {
            DelegateNode *d = hash_find(n->id, NULL, NULL);
            if (d && out) *out = d->data;
            return d ? DSA_OK : DSA_ERR_NOT_FOUND;
        }
        n = c < 0 ? n->left : n->right;
    }
    return DSA_ERR_NOT_FOUND;
}

/* Copies every record into an array (unsorted), the input for the
 * linear and binary searches. Caller frees. */
static DelegateRec *snapshot(int *n)
{
    DelegateRec *a;
    *n = g_count;
    if (g_count == 0) return NULL;
    a = (DelegateRec *)malloc((size_t)g_count * sizeof *a);
    if (!a) { *n = 0; return NULL; }
    delegate_to_array(a, g_count);
    return a;
}

int delegate_search_linear_id(const char *id, DelegateRec *out, SearchTrace *t)
{
    int i, n, rc = DSA_ERR_NOT_FOUND;
    DelegateRec *a;
    trace_reset(t);
    if (!id) return DSA_ERR_INVALID;
    a = snapshot(&n);
    for (i = 0; i < n; i++) {
        trace_visit(t, a[i].id);
        if (strcmp(a[i].id, id) == 0) {
            if (out) *out = a[i];
            rc = DSA_OK;
            break;
        }
    }
    free(a);
    return rc;
}

/* Binary search needs a sorted array: the snapshot is sorted by ID with
 * insertion sort first. Only the search's own comparisons are counted. */
int delegate_search_binary_id(const char *id, DelegateRec *out, SearchTrace *t)
{
    int n, i, lo, hi, rc = DSA_ERR_NOT_FOUND;
    DelegateRec *a, key;
    trace_reset(t);
    if (!id) return DSA_ERR_INVALID;
    a = snapshot(&n);
    for (i = 1; i < n; i++) {                   /* insertion sort by id */
        int j = i - 1;
        key = a[i];
        while (j >= 0 && strcmp(a[j].id, key.id) > 0) { a[j + 1] = a[j]; j--; }
        a[j + 1] = key;
    }
    lo = 0;
    hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2, c;
        trace_visit(t, a[mid].id);
        c = strcmp(id, a[mid].id);
        if (c == 0) { if (out) *out = a[mid]; rc = DSA_OK; break; }
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    free(a);
    return rc;
}

int delegate_search_text(const char *query, DelegateRec *out, int max, int *comparisons)
{
    int i, found = 0, cmp = 0;
    if (!query) query = "";
    for (i = 0; i < g_buckets; i++) {
        DelegateNode *n;
        for (n = g_table[i]; n; n = n->next) {
            cmp++;
            if (ci_contains(n->data.name, query) || ci_contains(n->data.id, query) ||
                ci_contains(n->data.institution, query) || ci_contains(n->data.portfolio, query)) {
                if (out && found < max) out[found] = n->data;
                found++;
            }
        }
    }
    if (comparisons) *comparisons = cmp;
    return found < max ? found : max;
}

/* ======================================================= FILE HANDLING */

int delegates_save(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    int i, k;
    FILE *f = pers_begin_save(path, "delegates", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (i = 0; i < g_buckets; i++) {
        DelegateNode *n;
        for (n = g_table[i]; n; n = n->next) {
            const DelegateRec *d = &n->data;
            pers_write_field(f, d->id);           fputc('|', f);
            pers_write_field(f, d->name);         fputc('|', f);
            pers_write_field(f, d->email);        fputc('|', f);
            pers_write_field(f, d->institution);  fputc('|', f);
            pers_write_field(f, d->conferenceId); fputc('|', f);
            for (k = 0; k < DELEGATE_PREFS; k++) { pers_write_field(f, d->prefs[k]); fputc('|', f); }
            pers_write_int(f, d->status);          fputc('|', f);
            pers_write_field(f, d->note);         fputc('|', f);
            pers_write_field(f, d->committeeId);  fputc('|', f);
            pers_write_field(f, d->portfolio);    fputc('|', f);
            pers_write_field(f, d->registeredAt); fputc('\n', f);
        }
    }
    return pers_finish_save(f, tmp, path);
}

/* Loads into a fresh table. On failure the previous table is restored. */
int delegates_load(const char *path, int *skipped)
{
    DelegateNode **oldTable = g_table;
    int oldBuckets = g_buckets, oldCount = g_count, oldRehashes = g_rehashes;
    BstNode *oldRoot = g_root;
    int bad = 0, r, rc, k;
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f;

    rc = pers_open_for_load(path, "delegates", &f);
    if (rc != DSA_OK) return rc;

    g_table = NULL; g_buckets = 0; g_count = 0; g_rehashes = 0; g_root = NULL;
    if (f) {
        while ((r = pers_read_line(f, line, sizeof line)) != 0) {
            DelegateRec d;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&d, 0, sizeof d);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 13) { bad++; continue; }
            pers_copy_field(d.id, fld[0], DSA_ID_LEN);
            pers_copy_field(d.name, fld[1], DSA_NAME_LEN);
            pers_copy_field(d.email, fld[2], DSA_NAME_LEN);
            pers_copy_field(d.institution, fld[3], DSA_NAME_LEN);
            pers_copy_field(d.conferenceId, fld[4], DSA_ID_LEN);
            for (k = 0; k < DELEGATE_PREFS; k++) pers_copy_field(d.prefs[k], fld[5 + k], DSA_ID_LEN);
            d.status = pers_parse_int(fld[8]);
            pers_copy_field(d.note, fld[9], DSA_TEXT_LEN);
            pers_copy_field(d.committeeId, fld[10], DSA_ID_LEN);
            pers_copy_field(d.portfolio, fld[11], DSA_NAME_LEN);
            pers_copy_field(d.registeredAt, fld[12], DSA_STAMP_LEN);
            rc = delegate_insert(&d);
            if (rc == DSA_ERR_NO_MEMORY) {
                fclose(f);
                delegate_store_reset();
                g_table = oldTable; g_buckets = oldBuckets; g_count = oldCount;
                g_rehashes = oldRehashes; g_root = oldRoot;
                return rc;
            }
            if (rc != DSA_OK) bad++;
        }
        fclose(f);
    }
    /* Success: free the previous structures */
    {
        DelegateNode **newTable = g_table;
        int newBuckets = g_buckets, newCount = g_count, newRehashes = g_rehashes;
        BstNode *newRoot = g_root;
        g_table = oldTable; g_buckets = oldBuckets; g_count = oldCount; g_root = oldRoot;
        delegate_store_reset();
        g_table = newTable; g_buckets = newBuckets; g_count = newCount;
        g_rehashes = newRehashes; g_root = newRoot;
    }
    if (skipped) *skipped += bad;
    return DSA_OK;
}
