// Module 1: Conference Management - DS Version (C)
// Data Structure: Stack for session history/undo

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

#define MAX_SESSIONS 50
#define MAX_HISTORY 100

struct Session {
    char sessionId[20];
    char name[100];
    int attendees;
};

struct HistoryEntry {
    char action[200];
    int timestamp;
};

struct Stack {
    struct HistoryEntry items[MAX_HISTORY];
    int top;
};

static struct Stack* create_stack(void) {
    struct Stack* s = (struct Stack*)malloc(sizeof(struct Stack));
    if (s == NULL) return NULL;   /* malloc can fail */
    s->top = -1;
    return s;
}

static void push_history(struct Stack* s, const char* action) {
    if (s->top < MAX_HISTORY - 1) {
        s->top++;
        strcpy(s->items[s->top].action, action);
        printf("Action recorded: %s\n", action);
    }
}

static void undo_history(struct Stack* s) {
    if (s->top >= 0) {
        printf("Undo: %s\n", s->items[s->top].action);
        s->top--;
    }
}

static void display_history(struct Stack* s) {
    printf("\n=== SESSION HISTORY (Most Recent First) ===\n");
    for (int i = s->top; i >= 0; i--) {
        printf("[%d] %s\n", i + 1, s->items[i].action);
    }
    printf("\n");
}

void module01_ds_demo(void) {
    printf("\n========== MODULE 1: DS - Conference with Stack History ==========\n\n");
    
    struct Stack* history = create_stack();
    
    if (history == NULL) { printf("Memory allocation failed\n"); return; }
    
    push_history(history, "Session 1 Created: Opening Ceremony");
    push_history(history, "Added 180 attendees to Session 1");
    push_history(history, "Session 2 Created: General Debate");
    push_history(history, "Added 150 attendees to Session 2");
    push_history(history, "Session 3 Created: Voting");
    
    display_history(history);
    
    printf("Performing undo operations...\n");
    undo_history(history);
    undo_history(history);
    
    display_history(history);
    
    free(history);
}