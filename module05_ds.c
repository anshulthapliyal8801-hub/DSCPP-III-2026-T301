// Module 5: Speakers - DS Version (C)
// Data Structure: Stack for speaker order

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

#define MAX_SPEAKERS 50

struct Speaker {
    char name[100];
    int speakingTime;
};

struct SpeakerStack {
    struct Speaker items[MAX_SPEAKERS];
    int top;
};

static struct SpeakerStack* create_stack(void) {
    struct SpeakerStack* s = (struct SpeakerStack*)malloc(sizeof(struct SpeakerStack));
    if (s == NULL) return NULL;
    s->top = -1;
    return s;
}

// push: add a speaker on top of the stack
static void push_speaker(struct SpeakerStack* s, const char* name, int time) {
    if (s->top < MAX_SPEAKERS - 1) {
        s->top++;
        strcpy(s->items[s->top].name, name);
        s->items[s->top].speakingTime = time;
        printf("Speaker added: %s (Time: %d s)\n", name, time);
    }
}

static void display_speakers(struct SpeakerStack* s) {
    printf("\n=== SPEAKERS STACK ===\n");
    for (int i = s->top; i >= 0; i--) {
        printf("%s - %d seconds\n", s->items[i].name, s->items[i].speakingTime);
    }
    printf("\n");
}

void module05_ds_demo(void) {
    printf("\n========== MODULE 5: DS - Speakers Stack ==========\n\n");
    struct SpeakerStack* stack = create_stack();
    if (stack == NULL) {
        printf("Memory allocation failed\n");
        return;
    }
    push_speaker(stack, "Aarav", 300);
    push_speaker(stack, "John", 250);
    push_speaker(stack, "Emma", 280);
    display_speakers(stack);
    free(stack);
}
