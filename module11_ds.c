// Module 11: Reports - DS Version (C)
// Data Structure: Queue (circular array) for report processing

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

#define MAX_REPORTS 100

struct Report {
    char title[200];
    char status[50];
};

struct ReportQueue {
    struct Report items[MAX_REPORTS];
    int front, rear, size;
};

static struct ReportQueue* create_rq(void) {
    struct ReportQueue* q = (struct ReportQueue*)malloc(sizeof(struct ReportQueue));
    if (q == NULL) return NULL;
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    return q;
}

// enqueue: add a report at the back
static void enqueue_report(struct ReportQueue* q, const char* title) {
    if (q->size < MAX_REPORTS) {
        q->rear = (q->rear + 1) % MAX_REPORTS;
        strcpy(q->items[q->rear].title, title);
        strcpy(q->items[q->rear].status, "Pending");
        q->size++;
    }
}

static void display_reports(struct ReportQueue* q) {
    printf("\n=== REPORT QUEUE ===\n");
    for (int i = 0; i < q->size; i++) {
        int idx = (q->front + i) % MAX_REPORTS;
        printf("%d. %s - %s\n", i + 1, q->items[idx].title, q->items[idx].status);
    }
    printf("\n");
}

void module11_ds_demo(void) {
    printf("\n========== MODULE 11: DS - Reports Queue ==========\n\n");
    struct ReportQueue* queue = create_rq();
    if (queue == NULL) {
        printf("Memory allocation failed\n");
        return;
    }
    enqueue_report(queue, "Attendance Report");
    enqueue_report(queue, "Voting Results");
    display_reports(queue);
    free(queue);
}
