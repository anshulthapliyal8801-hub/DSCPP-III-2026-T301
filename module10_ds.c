// Module 10: Executive Board - DS Version (C)
// Data Structure: Singly Linked List

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

struct EB {
    char name[100];
    char position[100];
    struct EB* next;
};

static struct EB* create_eb(const char* name, const char* pos) {
    struct EB* eb = (struct EB*)malloc(sizeof(struct EB));
    if (eb == NULL) return NULL;
    strcpy(eb->name, name);
    strcpy(eb->position, pos);
    eb->next = NULL;
    return eb;
}

// insert a new member at the end of the list
static struct EB* add_eb(struct EB* head, const char* name, const char* pos) {
    struct EB* new_eb = create_eb(name, pos);
    if (new_eb == NULL) return head;
    if (head == NULL) return new_eb;

    struct EB* curr = head;
    while (curr->next != NULL) curr = curr->next;
    curr->next = new_eb;
    return head;
}

static void display_eb(struct EB* head) {
    printf("\n=== EXECUTIVE BOARD ===\n");
    struct EB* curr = head;
    while (curr != NULL) {
        printf("%s - %s\n", curr->name, curr->position);
        curr = curr->next;
    }
    printf("\n");
}

// free every node of the list
static void free_eb_list(struct EB* head) {
    while (head != NULL) {
        struct EB* next = head->next;
        free(head);
        head = next;
    }
}

void module10_ds_demo(void) {
    printf("\n========== MODULE 10: DS - Executive Board Linked List ==========\n\n");
    struct EB* head = NULL;
    head = add_eb(head, "Aarav Sharma", "Secretary General");
    head = add_eb(head, "Emma Wilson", "Under Secretary");
    display_eb(head);
    free_eb_list(head);
}
