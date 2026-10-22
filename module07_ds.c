// Module 7: Voting - DS Version (C)
// Note: this uses a plain array of votes and counts them one by one.
// A real hash table for vote lookup is still planned.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

#define MAX_VOTES 200

struct Vote {
    char delegateId[20];
    char choice[20];
};

struct VoteTable {
    struct Vote items[MAX_VOTES];
    int count;
};

static struct VoteTable* create_votes(void) {
    struct VoteTable* v = (struct VoteTable*)malloc(sizeof(struct VoteTable));
    if (v == NULL) return NULL;
    v->count = 0;
    return v;
}

static void add_vote(struct VoteTable* v, const char* id, const char* choice) {
    if (v->count < MAX_VOTES) {
        strcpy(v->items[v->count].delegateId, id);
        strcpy(v->items[v->count].choice, choice);
        v->count++;
    }
}

static void display_votes(struct VoteTable* v) {
    printf("\n=== VOTING RESULTS ===\n");
    int for_count = 0, against_count = 0;
    for (int i = 0; i < v->count; i++) {
        printf("%s: %s\n", v->items[i].delegateId, v->items[i].choice);
        if (strcmp(v->items[i].choice, "For") == 0) {
            for_count++;
        } else {
            against_count++;
        }
    }
    printf("\nFor: %d | Against: %d\n\n", for_count, against_count);
}

void module07_ds_demo(void) {
    printf("\n========== MODULE 7: DS - Voting (array-based; hash table planned) ==========\n\n");
    struct VoteTable* votes = create_votes();
    if (votes == NULL) {
        printf("Memory allocation failed\n");
        return;
    }
    add_vote(votes, "D001", "For");
    add_vote(votes, "D002", "For");
    add_vote(votes, "D003", "Against");
    display_votes(votes);
    free(votes);
}
