// Module 3: Committees - DS Version (Linked List)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

struct Committee {
    char id[20];
    char name[100];
    int capacity;
    struct Committee* next;
};

static struct Committee* create_committee(const char* id, const char* name, int cap) {
    struct Committee* c = (struct Committee*)malloc(sizeof(struct Committee));
    if (c == NULL) return NULL;   /* malloc can fail */
    strcpy(c->id, id);
    strcpy(c->name, name);
    c->capacity = cap;
    c->next = NULL;
    return c;
}

static struct Committee* add_committee(struct Committee* head, const char* id, const char* name, int cap) {
    struct Committee* new_c = create_committee(id, name, cap);
    if (new_c == NULL) return head;
    if (!head) return new_c;
    struct Committee* curr = head;
    while (curr->next) curr = curr->next;
    curr->next = new_c;
    return head;
}

static void display_committees(struct Committee* head) {
    printf("\n=== COMMITTEES LINKED LIST ===\n");
    struct Committee* curr = head;
    while (curr) {
        printf("[%s] %s - Capacity: %d\n", curr->id, curr->name, curr->capacity);
        curr = curr->next;
    }
    printf("\n");
}

/* Release every node of the list (fixes the memory leak) */
static void free_committees(struct Committee* head) {
    while (head != NULL) {
        struct Committee* next = head->next;
        free(head);
        head = next;
    }
}

void module03_ds_demo(void) {
    printf("\n========== MODULE 3: DS - Committees Linked List ==========\n\n");
    struct Committee* head = NULL;
    head = add_committee(head, "COM1", "UNGA", 20);
    head = add_committee(head, "COM2", "UNHRC", 15);
    head = add_committee(head, "COM3", "AIPPM", 18);
    display_committees(head);
    free_committees(head);
}
