/*
 * MUNSphere — C tests: accounts (Shared), EB adjacency list (Module 10),
 * delegate hash table + BST (Module 2), roster algorithms (Module 3).
 * Run from the project root.
 */
#include <stdio.h>
#include <string.h>
#include "dsa_api.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { g_pass++; }                                           \
        else { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, msg); } \
    } while (0)

/* ------------------------------------------------------------ accounts */
static AccountRec acct(const char *id, int role, const char *name)
{
    AccountRec a;
    memset(&a, 0, sizeof a);
    strcpy(a.id, id);
    a.role = role;
    strcpy(a.name, name);
    strcpy(a.passHash, "pbkdf2-sha256$1000$abcd$ef01");
    a.active = 1;
    strcpy(a.createdAt, "2026-11-07 10:00:00");
    return a;
}

static void test_accounts(void)
{
    AccountRec a = acct("ADM001", ROLE_ADMIN, "Admin | One"), b = acct("D001", ROLE_DELEGATE, "Rahul"), out;
    LoginEventRec ev, hist[8];
    int i, n, cmp;
    printf("Accounts hash table and login history\n");
    account_store_reset();
    CHECK(account_find("ADM001", &out, &cmp) == DSA_ERR_NOT_FOUND && cmp == 0, "empty table");
    CHECK(account_insert(&a) == DSA_OK && account_insert(&b) == DSA_OK, "insert two");
    CHECK(account_insert(&a) == DSA_ERR_DUPLICATE, "duplicate rejected");
    a.role = 9;
    CHECK(account_insert(&a) == DSA_ERR_INVALID, "invalid role rejected");
    CHECK(account_find("D001", &out, &cmp) == DSA_OK && strcmp(out.name, "Rahul") == 0, "find");
    CHECK(cmp >= 1, "at least one comparison");
    out.active = 0;
    CHECK(account_update(&out) == DSA_OK, "update");
    account_find("D001", &out, NULL);
    CHECK(out.active == 0, "update visible");
    CHECK(account_count_role(ROLE_DELEGATE) == 1 && account_count_role(ROLE_ADMIN) == 1, "role counts");
    CHECK(account_max_id_suffix("D") == 1 && account_max_id_suffix("ADM") == 1, "id suffix");

    memset(&ev, 0, sizeof ev);
    strcpy(ev.accountId, "D001");
    for (i = 0; i < LOGIN_HISTORY_CAP + 5; i++) {
        snprintf(ev.detail, sizeof ev.detail, "event %d", i);
        ev.success = i % 2;
        login_history_add(&ev);
    }
    CHECK(login_history_count() == LOGIN_HISTORY_CAP, "history capped");
    n = login_history_to_array("D001", hist, 8);
    CHECK(n == 8 && strcmp(hist[0].detail, "event 1004") == 0, "newest first");
    CHECK(login_history_to_array("NOBODY", hist, 8) == 0, "filter by account");

    CHECK(accounts_save("tests/tmp_accounts.txt", "tests/tmp_history.txt") == DSA_OK, "save");
    account_store_reset();
    n = 0;
    CHECK(accounts_load("tests/tmp_accounts.txt", "tests/tmp_history.txt", &n) == DSA_OK && n == 0, "load");
    CHECK(account_find("ADM001", &out, NULL) == DSA_OK && strcmp(out.name, "Admin | One") == 0 &&
          strcmp(out.passHash, "pbkdf2-sha256$1000$abcd$ef01") == 0, "fields round-trip");
    CHECK(login_history_count() == LOGIN_HISTORY_CAP, "history reloaded");
    n = login_history_to_array(NULL, hist, 1);
    CHECK(n == 1 && strcmp(hist[0].detail, "event 1004") == 0, "history order kept");
    CHECK(account_delete("D001") == DSA_OK && account_count() == 1, "delete");
    remove("tests/tmp_accounts.txt");
    remove("tests/tmp_history.txt");
}

/* ------------------------------------------------------------------ EB */
static EbRec ebr(const char *id, const char *com, int role)
{
    EbRec e;
    memset(&e, 0, sizeof e);
    strcpy(e.id, id);
    strcpy(e.committeeId, com);
    e.ebRole = role;
    e.permissions = 7;
    return e;
}

static void test_eb(void)
{
    EbRec a = ebr("EB001", "COM001", EB_CHAIRPERSON), b = ebr("EB002", "COM001", EB_MODERATOR),
          c = ebr("EB003", "COM002", EB_CHAIRPERSON), out, list[8];
    int n, skipped = 0;
    printf("EB adjacency list\n");
    eb_store_reset();
    CHECK(eb_insert(&a) == DSA_OK && eb_insert(&b) == DSA_OK && eb_insert(&c) == DSA_OK, "insert");
    CHECK(eb_insert(&a) == DSA_ERR_DUPLICATE, "duplicate rejected");
    a.ebRole = 5;
    CHECK(eb_insert(&a) == DSA_ERR_INVALID, "bad role rejected");
    CHECK(eb_count() == 3 && eb_committee_count() == 2, "3 members, 2 committee vertices");
    CHECK(eb_count_by_committee("COM001") == 2 && eb_count_by_committee("COM002") == 1, "per committee");
    n = eb_list_by_committee("COM001", list, 8);
    CHECK(n == 2, "list by committee");

    c.committeeId[5] = '1';                       /* transfer EB003 to COM001 */
    CHECK(eb_update(&c) == DSA_OK, "transfer");
    CHECK(eb_count_by_committee("COM001") == 3 && eb_committee_count() == 1,
          "empty COM002 vertex removed");
    CHECK(eb_find("EB003", &out, NULL) == DSA_OK && strcmp(out.committeeId, "COM001") == 0, "moved");
    CHECK(eb_count() == 3, "count unchanged by transfer");

    CHECK(eb_save("tests/tmp_eb.txt") == DSA_OK, "save");
    eb_store_reset();
    CHECK(eb_load("tests/tmp_eb.txt", &skipped) == DSA_OK && eb_count() == 3 && skipped == 0, "load");
    CHECK(eb_delete("EB001") == DSA_OK && eb_delete("EB001") == DSA_ERR_NOT_FOUND, "delete");
    CHECK(eb_count() == 2, "count after delete");
    remove("tests/tmp_eb.txt");
}

/* ------------------------------------------------------------ delegates */
static DelegateRec del(const char *id, const char *name)
{
    DelegateRec d;
    memset(&d, 0, sizeof d);
    strcpy(d.id, id);
    strcpy(d.name, name);
    strcpy(d.institution, "Graphic Era");
    d.status = REGISTRATION_NONE;
    return d;
}

static int is_sorted_by_name(void)
{
    char ids[64][DSA_ID_LEN];
    int n = bst_inorder(ids, 64), i;
    DelegateRec a, b;
    for (i = 1; i < n; i++) {
        delegate_find(ids[i - 1], &a, NULL);
        delegate_find(ids[i], &b, NULL);
        if (strcmp(a.name, b.name) > 0) return 0;
    }
    return 1;
}

static void test_delegates(void)
{
    const char *names[] = {"Meera", "Arjun", "Zara", "Bilal", "Neha", "Yusuf", "Chen"};
    char id[DSA_ID_LEN], ids[64][DSA_ID_LEN];
    DelegateRec d, out;
    SearchTrace t;
    int i, n, skipped = 0;

    printf("Delegate hash table: rehashing and collisions\n");
    delegate_store_reset();
    CHECK(delegate_bucket_count() == 0 && delegate_load_factor() == 0.0, "empty table");
    for (i = 0; i < 6; i++) {
        snprintf(id, sizeof id, "D%03d", i + 1);
        d = del(id, names[i]);
        delegate_insert(&d);
    }
    CHECK(delegate_bucket_count() == 8 && delegate_rehash_count() == 0, "6 of 8: load 0.75, no rehash");
    d = del("D007", names[6]);
    delegate_insert(&d);
    CHECK(delegate_bucket_count() == 16 && delegate_rehash_count() == 1, "7th insert doubles to 16");
    CHECK(delegate_load_factor() < 0.75, "load factor back under limit");
    for (i = 1, n = 0; i <= 7; i++) {
        snprintf(id, sizeof id, "D%03d", i);
        if (delegate_find(id, NULL, NULL) == DSA_OK) n++;
    }
    CHECK(n == 7, "every record still reachable after rehash");
    d = del("D001", "Someone");
    CHECK(delegate_insert(&d) == DSA_ERR_DUPLICATE, "duplicate ID rejected");
    d = del("D099", "");
    CHECK(delegate_insert(&d) == DSA_ERR_INVALID, "empty name rejected");

    /* Force a collision: find two IDs in the same bucket */
    {
        int b0 = -1, longest = 0;
        char first[DSA_ID_LEN] = "", second[DSA_ID_LEN] = "";
        for (i = 100; i < 400 && !second[0]; i++) {
            snprintf(id, sizeof id, "X%d", i);
            if (b0 < 0) { b0 = (int)(delegate_hash_value(id) % 16u); strcpy(first, id); }
            else if ((int)(delegate_hash_value(id) % 16u) == b0) strcpy(second, id);
        }
        d = del(first, "Collision One");  delegate_insert(&d);
        d = del(second, "Collision Two"); delegate_insert(&d);
        b0 = (int)(delegate_hash_value(first) % (unsigned)delegate_bucket_count());
        longest = delegate_bucket_length(b0);
        CHECK(longest >= 2, "two keys chained in one bucket");
        CHECK(delegate_search_hash(second, &out, &t) == DSA_OK && strcmp(out.name, "Collision Two") == 0,
              "collided key found");
        CHECK(delegate_search_hash(first, &out, &t) == DSA_OK && t.comparisons >= 1, "other collided key found");
        CHECK(delegate_delete(first) == DSA_OK && delegate_delete(second) == DSA_OK, "delete collided keys");
    }

    printf("BST index: traversals, min/max, height, deletion cases\n");
    n = bst_inorder(ids, 64);
    CHECK(n == 7 && is_sorted_by_name(), "in-order traversal is alphabetical");
    CHECK(bst_node_count() == 7, "node count");
    {
        char mn[DSA_ID_LEN], mx[DSA_ID_LEN];
        bst_min(mn); bst_max(mx);
        CHECK(strcmp(mn, "D002") == 0 && strcmp(mx, "D003") == 0, "min Arjun, max Zara");
    }
    /* Insertion order Meera, Arjun, Zara, Bilal, Neha, Yusuf, Chen gives:
     *            Meera
     *          /       \
     *      Arjun       Zara
     *         \        /
     *        Bilal   Neha
     *           \       \
     *          Chen    Yusuf          height 4 */
    CHECK(bst_height() == 4, "height 4");
    n = bst_preorder(ids, 64);
    CHECK(n == 7 && strcmp(ids[0], "D001") == 0 && strcmp(ids[1], "D002") == 0, "pre-order starts at root");
    n = bst_postorder(ids, 64);
    CHECK(n == 7 && strcmp(ids[6], "D001") == 0, "post-order ends at root");
    {
        int parent[16], left[16];
        n = bst_level_order(ids, parent, left, 16);
        CHECK(n == 7 && parent[0] == -1 && strcmp(ids[1], "D002") == 0 && left[1] == 1 &&
              strcmp(ids[2], "D003") == 0 && left[2] == 0, "level order with parents");
    }
    CHECK(delegate_search_bst_name("neha", &out, &t) == DSA_OK && strcmp(out.id, "D005") == 0,
          "BST name search is case-insensitive");
    CHECK(t.comparisons == 3 && strcmp(t.path[0], "D001") == 0 && strcmp(t.path[2], "D005") == 0,
          "BST path Meera -> Zara -> Neha");

    CHECK(delegate_delete("D007") == DSA_OK, "delete leaf (Chen)");
    CHECK(delegate_delete("D002") == DSA_OK, "delete one-child node (Arjun)");
    CHECK(delegate_delete("D001") == DSA_OK, "delete two-child root (Meera)");
    n = bst_inorder(ids, 64);
    CHECK(n == 4 && is_sorted_by_name() && bst_node_count() == 4 && delegate_count() == 4,
          "tree still ordered after all three cases");
    n = bst_preorder(ids, 64);
    CHECK(strcmp(ids[0], "D005") == 0, "successor Neha replaced the root");

    delegate_find("D004", &out, NULL);
    strcpy(out.name, "Aaron");
    CHECK(delegate_update(&out) == DSA_OK, "rename");
    bst_min(id);
    CHECK(strcmp(id, "D004") == 0 && is_sorted_by_name(), "rename re-indexes the BST");

    printf("Linear and binary search\n");
    CHECK(delegate_search_linear_id("D006", &out, &t) == DSA_OK && t.comparisons >= 1 &&
          t.comparisons <= delegate_count(), "linear search");
    CHECK(delegate_search_linear_id("NOPE", &out, &t) == DSA_ERR_NOT_FOUND &&
          t.comparisons == delegate_count(), "unsuccessful linear search scans all");
    CHECK(delegate_search_binary_id("D006", &out, &t) == DSA_OK && strcmp(out.name, "Yusuf") == 0,
          "binary search finds");
    CHECK(delegate_search_binary_id("D000", &out, &t) == DSA_ERR_NOT_FOUND && t.comparisons <= 3,
          "binary search fails in at most log2(n)+1 steps");
    {
        DelegateRec res[8];
        int cmp;
        n = delegate_search_text("graphic", res, 8, &cmp);
        CHECK(n == 4 && cmp == 4, "text search over institution");
    }

    CHECK(delegates_save("tests/tmp_delegates.txt") == DSA_OK, "save");
    delegate_store_reset();
    CHECK(delegates_load("tests/tmp_delegates.txt", &skipped) == DSA_OK && delegate_count() == 4 &&
          skipped == 0, "load");
    CHECK(is_sorted_by_name() && bst_node_count() == 4, "BST rebuilt on load");
    {
        FILE *f = fopen("tests/tmp_bad.txt", "wb");
        fputs("not ours\n", f);
        fclose(f);
        CHECK(delegates_load("tests/tmp_bad.txt", &skipped) == DSA_ERR_INVALID && delegate_count() == 4,
              "bad file rejected, store untouched");
        remove("tests/tmp_bad.txt");
    }
    remove("tests/tmp_delegates.txt");

    printf("Large dataset (5000 delegates)\n");
    delegate_store_reset();
    for (i = 0, n = 0; i < 5000; i++) {
        snprintf(id, sizeof id, "D%04d", i);
        d = del(id, id);
        if (delegate_insert(&d) == DSA_OK) n++;
    }
    CHECK(n == 5000 && bst_node_count() == 5000, "5000 inserted");
    CHECK(delegate_load_factor() <= 0.75 && delegate_bucket_count() == 8192, "rehashed to 8192 buckets");
    delegate_search_hash("D4999", &out, &t);
    CHECK(t.comparisons <= 4, "hash lookup stays a few comparisons");
    delegate_search_binary_id("D4999", &out, &t);
    CHECK(t.comparisons <= 13, "binary search <= log2(5000)+1");
    delegate_search_linear_id("ZZZ", &out, &t);
    CHECK(t.comparisons == 5000, "linear worst case n");
    delegate_store_reset();
}

/* -------------------------------------------------------------- roster */
static void test_roster(void)
{
    RosterEntry r[4];
    SortStats st;
    printf("Roster sort and portfolio search\n");
    memset(r, 0, sizeof r);
    strcpy(r[0].id, "D3"); strcpy(r[0].name, "Zara");  strcpy(r[0].portfolio, "India");
    strcpy(r[1].id, "D1"); strcpy(r[1].name, "arjun"); strcpy(r[1].portfolio, "Brazil");
    strcpy(r[2].id, "D2"); strcpy(r[2].name, "Meera"); strcpy(r[2].portfolio, "China");
    strcpy(r[3].id, "D4"); strcpy(r[3].name, "Bilal"); strcpy(r[3].portfolio, "");
    roster_sort(r, 4, ROSTER_BY_NAME, &st);
    CHECK(strcmp(r[0].id, "D1") == 0 && strcmp(r[3].id, "D3") == 0, "by name, case-insensitive");
    CHECK(st.comparisons > 0 && st.moves > 0, "stats counted");
    roster_sort(r, 4, ROSTER_BY_NAME, &st);
    CHECK(st.comparisons == 3 && st.moves == 0, "sorted input: n-1 comparisons, 0 moves");
    roster_sort(r, 4, ROSTER_BY_PORTFOLIO, &st);
    CHECK(strcmp(r[0].id, "D4") == 0 && strcmp(r[1].portfolio, "Brazil") == 0, "by portfolio");
    CHECK(roster_find_portfolio(r, 4, "INDIA", NULL) >= 0, "portfolio found ignoring case");
    CHECK(roster_find_portfolio(r, 4, "India", "D3") == -1, "own entry ignored");
    CHECK(roster_find_portfolio(r, 4, "", NULL) == -1, "empty portfolio never conflicts");
    roster_sort(NULL, 0, ROSTER_BY_ID, &st);
    CHECK(st.comparisons == 0, "empty roster");
}

int main(void)
{
    printf("=== Accounts, EB, delegate and roster C tests ===\n");
    test_accounts();
    test_eb();
    test_delegates();
    test_roster();
    account_store_reset();
    eb_store_reset();
    delegate_store_reset();
    printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
