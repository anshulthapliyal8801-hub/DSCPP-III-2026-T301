/*
 * MUNSphere — C tests for Phase 4 structures:
 * attendance (Module 4), circular-queue GSL, request queue, POI queue
 * (Module 5), motion heap + undo stack (Module 6), voting (Module 7).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsa_api.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { g_pass++; }                                           \
        else { g_fail++; printf("  FAIL line %d: %s\n", __LINE__, msg); } \
    } while (0)

static void test_attendance(void)
{
    AttendanceRec a, list[256];
    int counts[ATT_STATUS_COUNT], upd = -1, i, n, skipped = 0;
    char id[DSA_ID_LEN];
    printf("Attendance sheets (dynamic arrays)\n");
    att_store_reset();
    memset(&a, 0, sizeof a);
    strcpy(a.sessionId, "SES001"); strcpy(a.delegateId, "D001"); a.status = ATT_CHECKED_IN;
    CHECK(att_mark(&a, &upd) == DSA_OK && upd == 0, "first mark inserts");
    a.status = ATT_PRESENT;
    CHECK(att_mark(&a, &upd) == DSA_OK && upd == 1, "second mark updates");
    CHECK(att_total() == 1, "no duplicate entry");
    a.status = 9;
    CHECK(att_mark(&a, NULL) == DSA_ERR_INVALID, "bad status rejected");
    for (i = 2; i <= 100; i++) {                 /* forces several doublings */
        snprintf(id, sizeof id, "D%03d", i);
        strcpy(a.delegateId, id);
        a.status = i % 3 == 0 ? ATT_ABSENT : (i % 3 == 1 ? ATT_PRESENT : ATT_LATE);
        att_mark(&a, NULL);
    }
    CHECK(att_session_tally("SES001", counts) == 100, "100 entries after growth");
    CHECK(counts[ATT_PRESENT] + counts[ATT_LATE] + counts[ATT_ABSENT] == 100, "tally sums");
    CHECK(att_find("SES001", "D100", &a, &n) == DSA_OK && n == 100, "linear search reaches the end");
    strcpy(a.sessionId, "SES002"); strcpy(a.delegateId, "D001"); a.status = ATT_LATE;
    att_mark(&a, NULL);
    n = att_list_delegate("D001", list, 256);
    CHECK(n == 2, "delegate appears in both sessions");
    CHECK(att_save("tests/tmp_att.txt") == DSA_OK, "save");
    att_store_reset();
    CHECK(att_load("tests/tmp_att.txt", &skipped) == DSA_OK && att_total() == 101 && skipped == 0, "reload");
    CHECK(att_find("SES002", "D001", &a, NULL) == DSA_OK && a.status == ATT_LATE, "status reloaded");
    remove("tests/tmp_att.txt");
}

static void test_gsl(void)
{
    char id[DSA_ID_LEN], out[DSA_ID_LEN], q[GSL_CAPACITY][DSA_ID_LEN];
    SpeakerState st;
    SpeechRec sp[8];
    int i, n, skipped = 0;
    printf("General Speakers List (circular queue)\n");
    spk_store_reset();
    CHECK(spk_state("SES001", &st) == DSA_OK && st.speakingSeconds == 90 && st.queueLength == 0, "defaults");
    CHECK(spk_next("SES001", 1000, out) == DSA_ERR_NOT_FOUND, "empty queue: no next speaker");
    for (i = 0; i < GSL_CAPACITY; i++) {
        snprintf(id, sizeof id, "D%03d", i);
        if (spk_enqueue("SES001", id) != DSA_OK) break;
    }
    CHECK(i == GSL_CAPACITY, "fills all 64 slots");
    CHECK(spk_enqueue("SES001", "X999") == DSA_ERR_FULL, "65th refused: full");
    CHECK(spk_enqueue("SES001", "D005") == DSA_ERR_DUPLICATE, "duplicate refused");
    for (i = 0; i < 10; i++) spk_next("SES001", 1000 + i * 60, out);
    CHECK(strcmp(out, "D009") == 0, "FIFO: 10th dequeued is D009");
    CHECK(spk_enqueue("SES001", "D009") == DSA_ERR_DUPLICATE, "current speaker can't re-queue");
    for (i = 0; i < 10; i++) {                   /* these wrap past the array end */
        snprintf(id, sizeof id, "W%02d", i);
        spk_enqueue("SES001", id);
    }
    n = spk_queue("SES001", q, GSL_CAPACITY);
    /* 54 still waiting (D010..D063) + 10 new = 64: full again, wrapped */
    CHECK(n == 64 && strcmp(q[0], "D010") == 0 && strcmp(q[53], "D063") == 0 && strcmp(q[63], "W09") == 0,
          "order correct across the wrap-around");
    CHECK(spk_enqueue("SES001", "X998") == DSA_ERR_FULL, "full again after wrapping");
    CHECK(spk_position("SES001", "W00") == 54, "position after wrap");
    CHECK(spk_remove("SES001", "D060") == DSA_OK, "remove near the wrap point");
    n = spk_queue("SES001", q, GSL_CAPACITY);
    CHECK(n == 63 && strcmp(q[50], "D061") == 0 && strcmp(q[53], "W00") == 0, "shifted across the wrap");
    CHECK(spk_set_time("SES001", 5) == DSA_ERR_INVALID && spk_set_time("SES001", 60) == DSA_OK, "time bounds");
    CHECK(speech_count() == 9, "9 finished speeches logged");
    n = speech_list("SES001", NULL, sp, 8);
    CHECK(n == 8 && strcmp(sp[0].delegateId, "D008") == 0 && sp[0].seconds == 60, "history newest first, length");
    CHECK(spk_finish("SES001", 2000) == DSA_OK && spk_finish("SES001", 2000) == DSA_ERR_NOT_FOUND, "finish once");

    CHECK(speakers_save("tests/tmp_g1.txt", "tests/tmp_g2.txt", "tests/tmp_g3.txt", "tests/tmp_g4.txt") == DSA_OK, "save");
    spk_store_reset();
    CHECK(speakers_load("tests/tmp_g1.txt", "tests/tmp_g2.txt", "tests/tmp_g3.txt", "tests/tmp_g4.txt", &skipped) == DSA_OK &&
          skipped == 0, "load");
    n = spk_queue("SES001", q, GSL_CAPACITY);
    CHECK(n == 63 && strcmp(q[0], "D010") == 0 && strcmp(q[62], "W09") == 0, "wrapped queue reloaded in order");
    spk_state("SES001", &st);
    CHECK(st.speakingSeconds == 60 && speech_count() == 10, "time and history reloaded");
}

static void test_requests_and_pois(void)
{
    SpeakRequestRec r, list[8];
    PoiRec p, pl[8];
    int i, n, skipped = 0;
    printf("Speaking requests (queue) and POIs (pending queue)\n");
    memset(&r, 0, sizeof r);
    strcpy(r.sessionId, "SES009");
    for (i = 1; i <= 3; i++) { snprintf(r.delegateId, DSA_ID_LEN, "D%03d", i); req_push(&r); }
    CHECK(req_push(&r) == DSA_ERR_DUPLICATE, "duplicate request refused");
    strcpy(r.sessionId, "SES010"); strcpy(r.delegateId, "D777"); req_push(&r);
    CHECK(req_pop_front("SES009", &r) == DSA_OK && strcmp(r.delegateId, "D001") == 0, "FIFO front");
    CHECK(req_remove("SES009", "D003") == DSA_OK, "withdraw from the middle/tail");
    n = req_list("SES009", list, 8);
    CHECK(n == 1 && strcmp(list[0].delegateId, "D002") == 0, "one left");
    CHECK(req_pop_front("SES009", &r) == DSA_OK && req_pop_front("SES009", &r) == DSA_ERR_NOT_FOUND, "drained");
    strcpy(r.sessionId, "SES011"); strcpy(r.delegateId, "D111");
    CHECK(req_push(&r) == DSA_OK && req_list("SES010", list, 8) == 1, "tail pointer still valid");

    memset(&p, 0, sizeof p);
    strcpy(p.sessionId, "SES001");
    for (i = 1; i <= 4; i++) {
        snprintf(p.id, DSA_ID_LEN, "P%03d", i);
        snprintf(p.delegateId, DSA_ID_LEN, "D%03d", i);
        strcpy(p.text, "Does the delegate agree | or not?");
        poi_insert(&p);
    }
    CHECK(poi_insert(&p) == DSA_ERR_DUPLICATE, "duplicate POI id");
    poi_find("P002", &p); p.status = POI_ACCEPTED;
    CHECK(poi_update(&p) == DSA_OK, "accept P002");
    n = poi_pending("SES001", pl, 8);
    CHECK(n == 3 && strcmp(pl[0].id, "P001") == 0 && strcmp(pl[1].id, "P003") == 0, "pending queue keeps FIFO, skips decided");
    n = poi_list("SES001", NULL, pl, 8);
    CHECK(n == 4 && strcmp(pl[0].id, "P004") == 0, "full list newest first");
    CHECK(poi_list(NULL, "D002", pl, 8) == 1 && pl[0].status == POI_ACCEPTED, "filter by delegate");
    CHECK(poi_max_id_suffix("P") == 4, "id suffix");
    speakers_save("tests/tmp_g1.txt", "tests/tmp_g2.txt", "tests/tmp_g3.txt", "tests/tmp_g4.txt");
    spk_store_reset();
    speakers_load("tests/tmp_g1.txt", "tests/tmp_g2.txt", "tests/tmp_g3.txt", "tests/tmp_g4.txt", &skipped);
    n = poi_pending("SES001", pl, 8);
    CHECK(n == 3 && strcmp(pl[0].id, "P001") == 0 && strcmp(pl[2].id, "P004") == 0, "pending order rebuilt on load");
    poi_find("P001", &p);
    CHECK(strcmp(p.text, "Does the delegate agree | or not?") == 0, "text with | round-trips");
    CHECK(req_list("SES011", list, 8) == 1, "requests reloaded");
    remove("tests/tmp_g1.txt"); remove("tests/tmp_g2.txt"); remove("tests/tmp_g3.txt"); remove("tests/tmp_g4.txt");
}

static MotionRec mk(const char *id, int type, int minutes)
{
    MotionRec m;
    memset(&m, 0, sizeof m);
    strcpy(m.id, id);
    strcpy(m.sessionId, "SES001");
    m.type = type;
    m.totalMinutes = minutes;
    m.status = MS_PENDING;
    m.seq = motion_next_seq();
    return m;
}

static int pending_sorted(void)
{
    MotionRec out[512];
    int n = motion_pending("SES001", out, 512), i;
    for (i = 1; i < n; i++) {
        if (out[i - 1].type > out[i].type) return 0;
        if (out[i - 1].type == out[i].type) {
            if (out[i - 1].totalMinutes < out[i].totalMinutes) return 0;
            if (out[i - 1].totalMinutes == out[i].totalMinutes && out[i - 1].seq > out[i].seq) return 0;
        }
    }
    return 1;
}

static void test_motions(void)
{
    MotionRec m, out[16];
    MotionActionRec a;
    int n, i, skipped = 0;
    char id[DSA_ID_LEN];
    printf("Motions: priority queue (heap) and undo stack\n");
    motion_store_reset();
    m = mk("M001", MOT_MODERATED, 10); motion_insert(&m);
    m = mk("M002", MOT_MODERATED, 15); motion_insert(&m);
    m = mk("M003", MOT_SPEAKING_TIME, 0); motion_insert(&m);
    m = mk("M004", MOT_SUSPEND, 0); motion_insert(&m);
    m = mk("M005", MOT_MODERATED, 15); motion_insert(&m);
    m = mk("M006", MOT_UNMODERATED, 5); motion_insert(&m);
    CHECK(motion_insert(&m) == DSA_ERR_DUPLICATE, "duplicate id");
    n = motion_pending("SES001", out, 16);
    CHECK(n == 6 && !strcmp(out[0].id, "M004") && !strcmp(out[1].id, "M006") && !strcmp(out[2].id, "M002") &&
          !strcmp(out[3].id, "M005") && !strcmp(out[4].id, "M001") && !strcmp(out[5].id, "M003"),
          "precedence, then longer first, then arrival order");
    CHECK(motion_peek("SES001", &m) == DSA_OK && !strcmp(m.id, "M004"), "peek top");
    CHECK(motion_heap_size("SES001") == 6, "pending motions do not get consumed by listing");
    motion_find("M002", &m); m.status = MS_WITHDRAWN;
    CHECK(motion_update(&m) == DSA_OK && motion_heap_size("SES001") == 5, "withdraw removes from the middle");
    CHECK(pending_sorted(), "heap order intact after removal");
    m.status = MS_PENDING;
    CHECK(motion_update(&m) == DSA_OK && motion_heap_size("SES001") == 6 && pending_sorted(), "re-entering heap");

    srand(7);                       /* 300 random operations keep heap order */
    for (i = 0; i < 300; i++) {
        snprintf(id, sizeof id, "R%03d", i);
        m = mk(id, rand() % MOTION_TYPE_COUNT, (rand() % 4) * 5);
        motion_insert(&m);
        if (i % 3 == 0) {
            snprintf(id, sizeof id, "R%03d", rand() % (i + 1));
            if (motion_find(id, &m) == DSA_OK && m.status == MS_PENDING) { m.status = MS_OUT_OF_ORDER; motion_update(&m); }
        }
    }
    CHECK(pending_sorted(), "heap order holds after 300 random inserts and removals");

    memset(&a, 0, sizeof a);
    strcpy(a.sessionId, "SES001");
    for (i = 1; i <= 3; i++) { snprintf(a.motionId, DSA_ID_LEN, "M00%d", i); a.toStatus = i; mact_push(&a); }
    CHECK(mact_depth("SES001") == 3, "stack depth 3");
    CHECK(mact_peek("SES001", &a) == DSA_OK && !strcmp(a.motionId, "M003"), "peek top = last pushed");
    CHECK(motions_save("tests/tmp_m1.txt", "tests/tmp_m2.txt") == DSA_OK, "save");
    {
        int before = motion_heap_size("SES001");
        motion_store_reset();
        CHECK(motions_load("tests/tmp_m1.txt", "tests/tmp_m2.txt", &skipped) == DSA_OK && skipped == 0, "load");
        CHECK(motion_heap_size("SES001") == before && pending_sorted(), "heap rebuilt from pending motions");
    }
    CHECK(mact_pop("SES001", &a) == DSA_OK && !strcmp(a.motionId, "M003"), "LIFO after reload: M003");
    CHECK(mact_pop("SES001", &a) == DSA_OK && !strcmp(a.motionId, "M002"), "then M002");
    mact_pop("SES001", &a);
    CHECK(mact_pop("SES001", &a) == DSA_ERR_NOT_FOUND, "empty stack");
    m = mk("M900", MOT_ADJOURN, 0); motion_insert(&m);
    CHECK(motion_find("M900", &m) == DSA_OK && m.seq > 306, "sequence continues after reload");
    CHECK(pending_sorted(), "still ordered with the new motion");
    remove("tests/tmp_m1.txt"); remove("tests/tmp_m2.txt");
}

static void test_votes(void)
{
    VoteRec v;
    BallotRec b, list[8];
    int counts[VOTE_CHOICE_COUNT], i, skipped = 0, cmp;
    char id[DSA_ID_LEN];
    printf("Voting: ballots, duplicates, corrections, tally\n");
    vote_store_reset();
    memset(&v, 0, sizeof v);
    strcpy(v.id, "V001"); strcpy(v.sessionId, "SES001"); strcpy(v.subject, "Draft resolution 1.1");
    v.substantive = 1; v.outcome = -1;
    CHECK(vote_open(&v) == DSA_OK && vote_open(&v) == DSA_ERR_DUPLICATE, "open once");
    CHECK(vote_open_for_session("SES001", &v) == DSA_OK, "found open vote");
    memset(&b, 0, sizeof b);
    strcpy(b.voteId, "V001");
    for (i = 1; i <= 20; i++) {                   /* grows the ballot array */
        snprintf(id, sizeof id, "D%03d", i);
        strcpy(b.delegateId, id);
        b.choice = i <= 12 ? VOTE_YES : (i <= 17 ? VOTE_NO : VOTE_ABSTAIN);
        ballot_cast(&b);
    }
    strcpy(b.delegateId, "D001");
    CHECK(ballot_cast(&b) == DSA_ERR_DUPLICATE, "second ballot refused");
    b.choice = 7;
    strcpy(b.delegateId, "D099");
    CHECK(ballot_cast(&b) == DSA_ERR_INVALID, "bad choice refused");
    CHECK(vote_tally("V001", counts) == 20 && counts[0] == 12 && counts[1] == 5 && counts[2] == 3, "tally 12/5/3");
    CHECK(ballot_correct("V001", "D001", VOTE_NO) == DSA_OK, "correction");
    CHECK(ballot_find("V001", "D001", &b, &cmp) == DSA_OK && b.choice == VOTE_NO && b.corrections == 1 && cmp == 1,
          "correction recorded and counted");
    CHECK(ballot_correct("V001", "D404", VOTE_NO) == DSA_ERR_NOT_FOUND, "can't correct a missing ballot");
    vote_tally("V001", counts);
    CHECK(counts[0] == 11 && counts[1] == 6, "tally reflects correction");
    v.state = VS_CLOSED; v.yes = 11; v.no = 6; v.abstain = 3; v.outcome = 1;
    vote_update(&v);
    CHECK(vote_open_for_session("SES001", &v) == DSA_ERR_NOT_FOUND, "no open vote after close");
    CHECK(votes_save("tests/tmp_v1.txt", "tests/tmp_v2.txt") == DSA_OK, "save");
    vote_store_reset();
    CHECK(votes_load("tests/tmp_v1.txt", "tests/tmp_v2.txt", &skipped) == DSA_OK && skipped == 0, "load");
    CHECK(vote_find("V001", &v) == DSA_OK && v.outcome == 1 && v.yes == 11, "result reloaded");
    CHECK(ballot_list("V001", list, 8) == 8 && vote_tally("V001", counts) == 20, "ballots reloaded");
    CHECK(vote_delete("V001") == DSA_OK && vote_find("V001", NULL) == DSA_ERR_NOT_FOUND, "delete");
    remove("tests/tmp_v1.txt"); remove("tests/tmp_v2.txt");
}

int main(void)
{
    printf("=== Phase 4 C tests: attendance, speakers, POIs, motions, voting ===\n");
    test_attendance();
    test_gsl();
    test_requests_and_pois();
    test_motions();
    test_votes();
    att_store_reset(); spk_store_reset(); motion_store_reset(); vote_store_reset();
    printf("=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
