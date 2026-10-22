// Module 6: Motions - DS Version (C)
// Data Structure: Singly Linked List

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

struct Motion {
    char id[20];
    char title[200];
    struct Motion* next;
};

static struct Motion* create_motion(const char* id, const char* title) {
    struct Motion* m = (struct Motion*)malloc(sizeof(struct Motion));
    if (m == NULL) return NULL;
    strcpy(m->id, id);
    strcpy(m->title, title);
    m->next = NULL;
    return m;
}

// insert a new motion at the end of the list
static struct Motion* add_motion(struct Motion* head, const char* id, const char* title) {
    struct Motion* new_m = create_motion(id, title);
    if (new_m == NULL) return head;
    if (head == NULL) return new_m;

    struct Motion* curr = head;
    while (curr->next != NULL) curr = curr->next;
    curr->next = new_m;
    return head;
}

static void display_motions(struct Motion* head) {
    printf("\n=== MOTIONS ===\n");
    struct Motion* curr = head;
    while (curr != NULL) {
        printf("[%s] %s\n", curr->id, curr->title);
        curr = curr->next;
    }
    printf("\n");
}

// free every node of the list
static void free_motions(struct Motion* head) {
    while (head != NULL) {
        struct Motion* next = head->next;
        free(head);
        head = next;
    }
}

void module06_ds_demo(void) {
    printf("\n========== MODULE 6: DS - Motions Linked List ==========\n\n");
    struct Motion* head = NULL;
    head = add_motion(head, "M1", "Climate Action");
    head = add_motion(head, "M2", "Education Reform");
    display_motions(head);
    free_motions(head);
}
