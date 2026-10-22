


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

#define MAX_QUEUE 100

struct Delegate {
    char id[10];
    char name[100];
    char country[50];
    int position;
};

struct DelegateQueue {
    struct Delegate items[MAX_QUEUE];
    int front, rear, size;
};

static struct DelegateQueue* create_queue(void) {
    struct DelegateQueue* q = (struct DelegateQueue*)malloc(sizeof(struct DelegateQueue));
    if (q == NULL) return NULL;   /* malloc can fail */
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    return q;
}

static void enqueue_delegate(struct DelegateQueue* q, const char* id, const char* name, const char* country) {
    if (q->size < MAX_QUEUE) {
        q->rear = (q->rear + 1) % MAX_QUEUE;
        strcpy(q->items[q->rear].id, id);
        strcpy(q->items[q->rear].name, name);
        strcpy(q->items[q->rear].country, country);
        q->items[q->rear].position = ++q->size;
        printf("Delegate %s added to queue (Position: %d)\n", name, q->size);
    }
}

static void dequeue_delegate(struct DelegateQueue* q) {
    if (q->size > 0) {
        printf("Delegate %s removed from queue\n", q->items[q->front].name);
        q->front = (q->front + 1) % MAX_QUEUE;
        q->size--;
    }
}

static void display_queue(struct DelegateQueue* q) {
    printf("\n=== DELEGATES QUEUE ===\n");
    printf("%-10s %-25s %-20s %-10s\n", "ID", "Name", "Country", "Position");
    printf("-------------------------------------------\n");
    
    for (int i = 0; i < q->size; i++) {
        int idx = (q->front + i) % MAX_QUEUE;
        printf("%-10s %-25s %-20s %-10d\n",
               q->items[idx].id, q->items[idx].name, 
               q->items[idx].country, q->items[idx].position);
    }
    printf("\nQueue Size: %d\n\n", q->size);
}

void module02_ds_demo(void) {
    printf("\n========== MODULE 2: DS - Delegates Queue ==========\n\n");
    
    struct DelegateQueue* queue = create_queue();
    
    if (queue == NULL) { printf("Memory allocation failed\n"); return; }
    
    enqueue_delegate(queue, "D001", "Aarav Sharma", "India");
    enqueue_delegate(queue, "D002", "John Smith", "USA");
    enqueue_delegate(queue, "D003", "Emma Wilson", "UK");
    enqueue_delegate(queue, "D004", "Yuki Tanaka", "Japan");
    
    display_queue(queue);
    
    printf("Processing delegates...\n");
    dequeue_delegate(queue);
    dequeue_delegate(queue);
    
    display_queue(queue);
    
    free(queue);
}
