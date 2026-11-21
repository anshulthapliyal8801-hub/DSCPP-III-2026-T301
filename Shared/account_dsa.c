/*
 * MUNSphere — Shared: account store (C)
 *
 *   Hash table, separate chaining, 101 buckets (a prime, so IDs with
 *   common prefixes spread well). Every login looks an account up by ID,
 *   so lookup should be O(1) on average rather than O(n).
 *
 *   Login history: singly linked list, newest at the head (O(1) insert),
 *   capped at LOGIN_HISTORY_CAP by trimming the oldest node.
 *
 * Passwords are never stored: only "pbkdf2-sha256$iterations$salt$hash",
 * produced by Auth.cpp.
 */
#include <stdlib.h>
#include <string.h>

#include "dsa_api.h"

#define ACCOUNT_BUCKETS 101

typedef struct AccountNode {
    AccountRec data;
    struct AccountNode *next;
} AccountNode;

typedef struct HistoryNode {
    LoginEventRec data;
    struct HistoryNode *next;
} HistoryNode;

static AccountNode *g_buckets[ACCOUNT_BUCKETS];
static int g_accountCount;
static HistoryNode *g_history;      /* newest first */
static int g_historyCount;

/* djb2 string hash (Dan Bernstein): h = h * 33 + c */
static unsigned hash_id(const char *s)
{
    unsigned h = 5381u;
    while (*s) h = h * 33u + (unsigned char)*s++;
    return h % ACCOUNT_BUCKETS;
}

static void terminate_account(AccountRec *r)
{
    r->id[DSA_ID_LEN - 1] = '\0';
    r->name[DSA_NAME_LEN - 1] = '\0';
    r->passHash[DSA_PASS_LEN - 1] = '\0';
    r->createdAt[DSA_STAMP_LEN - 1] = '\0';
}

static AccountNode *find_node(AccountNode *const *buckets, const char *id,
                              AccountNode **prevOut, int *cmpOut)
{
    AccountNode *prev = NULL, *cur = buckets[hash_id(id)];
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

static int insert_into(AccountNode **buckets, int *count, const AccountRec *rec)
{
    AccountNode *n;
    unsigned b;
    if (!rec || rec->id[0] == '\0') return DSA_ERR_INVALID;
    if (rec->role < 0 || rec->role >= ROLE_COUNT) return DSA_ERR_INVALID;
    if (find_node(buckets, rec->id, NULL, NULL)) return DSA_ERR_DUPLICATE;
    n = (AccountNode *)malloc(sizeof *n);
    if (!n) return DSA_ERR_NO_MEMORY;
    n->data = *rec;
    terminate_account(&n->data);
    b = hash_id(rec->id);
    n->next = buckets[b];            /* insert at the head of the chain */
    buckets[b] = n;
    (*count)++;
    return DSA_OK;
}

static void free_buckets(AccountNode **buckets)
{
    int i;
    for (i = 0; i < ACCOUNT_BUCKETS; i++) {
        AccountNode *cur = buckets[i], *next;
        while (cur) { next = cur->next; free(cur); cur = next; }
        buckets[i] = NULL;
    }
}

static void free_history(HistoryNode **head)
{
    HistoryNode *cur = *head, *next;
    while (cur) { next = cur->next; free(cur); cur = next; }
    *head = NULL;
}

void account_store_reset(void)
{
    free_buckets(g_buckets);
    g_accountCount = 0;
    free_history(&g_history);
    g_historyCount = 0;
}

int account_insert(const AccountRec *rec)
{
    return insert_into(g_buckets, &g_accountCount, rec);
}

int account_find(const char *id, AccountRec *out, int *comparisons)
{
    AccountNode *n;
    if (!id) return DSA_ERR_INVALID;
    n = find_node(g_buckets, id, NULL, comparisons);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (out) *out = n->data;
    return DSA_OK;
}

int account_update(const AccountRec *rec)
{
    AccountNode *n;
    if (!rec) return DSA_ERR_INVALID;
    if (rec->role < 0 || rec->role >= ROLE_COUNT) return DSA_ERR_INVALID;
    n = find_node(g_buckets, rec->id, NULL, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    n->data = *rec;
    terminate_account(&n->data);
    return DSA_OK;
}

int account_delete(const char *id)
{
    AccountNode *prev, *n;
    if (!id) return DSA_ERR_INVALID;
    n = find_node(g_buckets, id, &prev, NULL);
    if (!n) return DSA_ERR_NOT_FOUND;
    if (prev) prev->next = n->next;
    else g_buckets[hash_id(id)] = n->next;
    free(n);
    g_accountCount--;
    return DSA_OK;
}

int account_count(void) { return g_accountCount; }

int account_count_role(int role)
{
    int i, c = 0;
    for (i = 0; i < ACCOUNT_BUCKETS; i++) {
        AccountNode *n;
        for (n = g_buckets[i]; n; n = n->next)
            if (n->data.role == role) c++;
    }
    return c;
}

/* Bucket order is not meaningful; C++ sorts for display. */
int account_to_array(AccountRec *out, int max)
{
    int i, k = 0;
    if (!out || max <= 0) return 0;
    for (i = 0; i < ACCOUNT_BUCKETS && k < max; i++) {
        AccountNode *n;
        for (n = g_buckets[i]; n && k < max; n = n->next) out[k++] = n->data;
    }
    return k;
}

int account_max_id_suffix(const char *prefix)
{
    int i, best = 0;
    size_t pl;
    if (!prefix) return 0;
    pl = strlen(prefix);
    for (i = 0; i < ACCOUNT_BUCKETS; i++) {
        AccountNode *n;
        for (n = g_buckets[i]; n; n = n->next) {
            const char *p = n->data.id;
            long v = 0;
            if (strncmp(p, prefix, pl) != 0 || p[pl] == '\0') continue;
            for (p += pl; *p >= '0' && *p <= '9' && v < 1000000000L; p++) v = v * 10 + (*p - '0');
            if (*p == '\0' && v > best) best = (int)v;
        }
    }
    return best;
}

/* ------------------------------------------------------ login history */

static void history_push(HistoryNode **head, int *count, const LoginEventRec *ev,
                         int atTail, HistoryNode **tail)
{
    HistoryNode *n = (HistoryNode *)malloc(sizeof *n);
    if (!n) return;                    /* history is best effort */
    n->data = *ev;
    n->data.accountId[DSA_ID_LEN - 1] = '\0';
    n->data.at[DSA_STAMP_LEN - 1] = '\0';
    n->data.detail[sizeof n->data.detail - 1] = '\0';
    n->next = NULL;
    if (atTail) {                      /* used while loading (file is newest first) */
        if (*tail) (*tail)->next = n; else *head = n;
        *tail = n;
    } else {
        n->next = *head;
        *head = n;
    }
    (*count)++;
}

/* Remove nodes beyond the cap: walk to node #cap and cut the rest. */
static void history_trim(HistoryNode **head, int *count)
{
    HistoryNode *cur = *head, *rest;
    int i;
    if (*count <= LOGIN_HISTORY_CAP) return;
    for (i = 1; i < LOGIN_HISTORY_CAP && cur; i++) cur = cur->next;
    if (!cur) return;
    rest = cur->next;
    cur->next = NULL;
    free_history(&rest);
    *count = LOGIN_HISTORY_CAP;
}

void login_history_add(const LoginEventRec *ev)
{
    if (!ev) return;
    history_push(&g_history, &g_historyCount, ev, 0, NULL);
    history_trim(&g_history, &g_historyCount);
}

int login_history_count(void) { return g_historyCount; }

int login_history_to_array(const char *accountId, LoginEventRec *out, int max)
{
    HistoryNode *n;
    int k = 0;
    if (!out || max <= 0) return 0;
    for (n = g_history; n && k < max; n = n->next)
        if (!accountId || !*accountId || strcmp(n->data.accountId, accountId) == 0)
            out[k++] = n->data;
    return k;
}

/* --------------------------------------------------------- file handling */

int accounts_save(const char *accountsPath, const char *historyPath)
{
    char tmp[DSA_PATH_LEN + 8];
    int i, rc;
    HistoryNode *h;
    FILE *f = pers_begin_save(accountsPath, "accounts", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (i = 0; i < ACCOUNT_BUCKETS; i++) {
        AccountNode *n;
        for (n = g_buckets[i]; n; n = n->next) {
            pers_write_field(f, n->data.id);        fputc('|', f);
            pers_write_int(f, n->data.role);         fputc('|', f);
            pers_write_field(f, n->data.name);      fputc('|', f);
            pers_write_field(f, n->data.passHash);  fputc('|', f);
            pers_write_int(f, n->data.active);       fputc('|', f);
            pers_write_int(f, n->data.mustChangePassword); fputc('|', f);
            pers_write_int(f, n->data.resetRequested);     fputc('|', f);
            pers_write_field(f, n->data.createdAt); fputc('\n', f);
        }
    }
    rc = pers_finish_save(f, tmp, accountsPath);
    if (rc != DSA_OK) return rc;

    f = pers_begin_save(historyPath, "login_history", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (h = g_history; h; h = h->next) {
        pers_write_field(f, h->data.accountId); fputc('|', f);
        pers_write_field(f, h->data.at);        fputc('|', f);
        pers_write_int(f, h->data.success);      fputc('|', f);
        pers_write_field(f, h->data.detail);    fputc('\n', f);
    }
    return pers_finish_save(f, tmp, historyPath);
}

int accounts_load(const char *accountsPath, const char *historyPath, int *skipped)
{
    AccountNode *buckets[ACCOUNT_BUCKETS] = {0};
    HistoryNode *hist = NULL, *tail = NULL;
    int count = 0, hcount = 0, bad = 0, r, rc;
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f;

    rc = pers_open_for_load(accountsPath, "accounts", &f);
    if (rc != DSA_OK) return rc;
    if (f) {
        while ((r = pers_read_line(f, line, sizeof line)) != 0) {
            AccountRec a;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&a, 0, sizeof a);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 8) { bad++; continue; }
            pers_copy_field(a.id, fld[0], DSA_ID_LEN);
            a.role = pers_parse_int(fld[1]);
            pers_copy_field(a.name, fld[2], DSA_NAME_LEN);
            pers_copy_field(a.passHash, fld[3], DSA_PASS_LEN);
            a.active = pers_parse_int(fld[4]);
            a.mustChangePassword = pers_parse_int(fld[5]);
            a.resetRequested = pers_parse_int(fld[6]);
            pers_copy_field(a.createdAt, fld[7], DSA_STAMP_LEN);
            if (a.active < 0 || a.active > 1 || a.mustChangePassword < 0 ||
                a.mustChangePassword > 1 || a.resetRequested < 0 || a.resetRequested > 1) {
                bad++;
                continue;
            }
            rc = insert_into(buckets, &count, &a);
            if (rc == DSA_ERR_NO_MEMORY) { fclose(f); free_buckets(buckets); return rc; }
            if (rc != DSA_OK) bad++;
        }
        fclose(f);
    }

    rc = pers_open_for_load(historyPath, "login_history", &f);
    if (rc != DSA_OK) { free_buckets(buckets); return rc; }
    if (f) {
        while ((r = pers_read_line(f, line, sizeof line)) != 0) {
            LoginEventRec e;
            if (r < 0) { bad++; continue; }
            if (!line[0]) continue;
            memset(&e, 0, sizeof e);
            if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 4) { bad++; continue; }
            pers_copy_field(e.accountId, fld[0], DSA_ID_LEN);
            pers_copy_field(e.at, fld[1], DSA_STAMP_LEN);
            e.success = pers_parse_int(fld[2]);
            pers_copy_field(e.detail, fld[3], sizeof e.detail);
            if (e.success < 0 || e.success > 1) { bad++; continue; }
            if (hcount < LOGIN_HISTORY_CAP) history_push(&hist, &hcount, &e, 1, &tail);
        }
        fclose(f);
    }

    account_store_reset();
    memcpy(g_buckets, buckets, sizeof g_buckets);
    g_accountCount = count;
    g_history = hist;
    g_historyCount = hcount;
    if (skipped) *skipped += bad;
    return DSA_OK;
}
