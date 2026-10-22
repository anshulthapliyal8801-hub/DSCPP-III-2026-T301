// Module 12: Persistence & Audit - DS Version (C)
// Data Structure: Stack for the log history

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

#define MAX_LOGS 500

struct LogEntry {
    char timestamp[50];
    char action[200];
};

struct LogStack {
    struct LogEntry items[MAX_LOGS];
    int top;
};

static struct LogStack* create_logstack(void) {
    struct LogStack* ls = (struct LogStack*)malloc(sizeof(struct LogStack));
    if (ls == NULL) return NULL;
    ls->top = -1;
    return ls;
}

// push: add a log entry on top of the stack
static void push_log(struct LogStack* ls, const char* timestamp, const char* action) {
    if (ls->top < MAX_LOGS - 1) {
        ls->top++;
        strcpy(ls->items[ls->top].timestamp, timestamp);
        strcpy(ls->items[ls->top].action, action);
    }
}

static void display_logs(struct LogStack* ls) {
    printf("\n=== AUDIT LOG (Most Recent First) ===\n");
    for (int i = ls->top; i >= 0; i--) {
        printf("[%s] %s\n", ls->items[i].timestamp, ls->items[i].action);
    }
    printf("\n");
}

void module12_ds_demo(void) {
    printf("\n========== MODULE 12: DS - Audit Log Stack ==========\n\n");
    struct LogStack* logs = create_logstack();
    if (logs == NULL) {
        printf("Memory allocation failed\n");
        return;
    }
    push_log(logs, "2026-10-15 09:00", "Conference Created");
    push_log(logs, "2026-10-15 09:15", "Session Added");
    push_log(logs, "2026-10-16 10:00", "Voting Started");
    display_logs(logs);
    free(logs);
}
