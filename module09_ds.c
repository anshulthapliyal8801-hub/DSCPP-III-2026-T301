// Module 9: Awards - DS Version (C)
// Data Structure: Stack for award order

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

#define MAX_AWARDS 50

struct Award {
    char delegateName[100];
    char awardType[50];
};

struct AwardStack {
    struct Award items[MAX_AWARDS];
    int top;
};

static struct AwardStack* create_award_stack(void) {
    struct AwardStack* s = (struct AwardStack*)malloc(sizeof(struct AwardStack));
    if (s == NULL) return NULL;
    s->top = -1;
    return s;
}

// push: add an award on top of the stack
static void push_award(struct AwardStack* s, const char* name, const char* type) {
    if (s->top < MAX_AWARDS - 1) {
        s->top++;
        strcpy(s->items[s->top].delegateName, name);
        strcpy(s->items[s->top].awardType, type);
    }
}

static void display_awards(struct AwardStack* s) {
    printf("\n=== AWARDS STACK ===\n");
    for (int i = s->top; i >= 0; i--) {
        printf("🏆 %s - %s\n", s->items[i].delegateName, s->items[i].awardType);
    }
    printf("\n");
}

void module09_ds_demo(void) {
    printf("\n========== MODULE 9: DS - Awards Stack ==========\n\n");
    struct AwardStack* stack = create_award_stack();
    if (stack == NULL) {
        printf("Memory allocation failed\n");
        return;
    }
    push_award(stack, "Aarav Sharma", "Gold");
    push_award(stack, "Emma Wilson", "Gold");
    push_award(stack, "John Smith", "Silver");
    display_awards(stack);
    free(stack);
}
