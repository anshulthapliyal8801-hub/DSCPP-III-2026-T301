// Module 4: Attendance - DS (array of records)
// NOTE: despite the struct name, this is a plain array with sequential
// insertion. A real hash table (hash function + collision handling) is pending.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/munsphere_modules.h"

#define MAX_RECORDS 100

struct AttRecord { char name[100]; int present; };
struct AttHashTable { struct AttRecord items[MAX_RECORDS]; int count; };

static struct AttHashTable* create_att(void) {
    struct AttHashTable* t = (struct AttHashTable*)malloc(sizeof(struct AttHashTable));
    if (t == NULL) return NULL;   /* malloc can fail */
    t->count = 0;
    return t;
}

static void mark_attendance(struct AttHashTable* t, const char* name, int present) {
    if (t->count < MAX_RECORDS) {
        strcpy(t->items[t->count].name, name);
        t->items[t->count].present = present;
        t->count++;
        printf("Marked: %s - %s\n", name, present ? "Present" : "Absent");
    }
}

static void display_attendance(struct AttHashTable* t) {
    printf("\n=== ATTENDANCE RECORDS ===\n");
    for (int i = 0; i < t->count; i++) {
        printf("%s: %s\n", t->items[i].name, t->items[i].present ? "Present" : "Absent");
    }
    printf("\n");
}

void module04_ds_demo(void) {
    printf("\n========== MODULE 4: DS - Attendance (array-based; hash table planned) ==========\n\n");
    struct AttHashTable* table = create_att();
    if (table == NULL) { printf("Memory allocation failed\n"); return; }
    mark_attendance(table, "Aarav", 1);
    mark_attendance(table, "John", 1);
    mark_attendance(table, "Emma", 0);
    display_attendance(table);
    free(table);
}