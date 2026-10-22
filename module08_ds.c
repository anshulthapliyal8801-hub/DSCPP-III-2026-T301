// Module 8: Scoring - DS Version (C)
// Data Structure: Array with Bubble Sort

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

struct Score {
    char name[100];
    int points;
};

static void swap_scores(struct Score* a, struct Score* b) {
    struct Score temp = *a;
    *a = *b;
    *b = temp;
}

// bubble sort: highest points first
static void bubble_sort_scores(struct Score* arr, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j].points < arr[j + 1].points) {
                swap_scores(&arr[j], &arr[j + 1]);
            }
        }
    }
}

static void display_scores(struct Score* arr, int n) {
    printf("\n=== SCORES (Sorted) ===\n");
    for (int i = 0; i < n; i++) {
        printf("%d. %s: %d points\n", i + 1, arr[i].name, arr[i].points);
    }
    printf("\n");
}

void module08_ds_demo(void) {
    printf("\n========== MODULE 8: DS - Scoring with Bubble Sort ==========\n\n");
    struct Score scores[3] = {
        {"Aarav", 85},
        {"John", 78},
        {"Emma", 92}
    };
    bubble_sort_scores(scores, 3);
    display_scores(scores, 3);
}
