// Module 8: Scoring - COMBINED (C++ class + Bubble Sort)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

class Score {
public:
    string name;
    int points;

    Score(string n, int p) {
        name = n;
        points = p;
    }

    void display() {
        cout << name << ": " << points << " pts\n";
    }
};

class ScoringList {
private:
    Score* scores[100];
    int count;

public:
    ScoringList() { count = 0; }

    // destructor: delete the objects created with new
    ~ScoringList() {
        for (int i = 0; i < count; i++) {
            delete scores[i];
        }
    }

    void add(string name, int points) {
        if (count < 100) {
            scores[count] = new Score(name, points);
            count++;
        }
    }

    // bubble sort: highest points first
    void sort() {
        for (int i = 0; i < count - 1; i++) {
            for (int j = 0; j < count - i - 1; j++) {
                if (scores[j]->points < scores[j + 1]->points) {
                    Score* temp = scores[j];
                    scores[j] = scores[j + 1];
                    scores[j + 1] = temp;
                }
            }
        }
    }

    void display() {
        cout << "\n=== SCORES (Sorted) ===\n";
        for (int i = 0; i < count; i++) {
            scores[i]->display();
        }
    }
};

}

void module08_combined_demo() {
    cout << "\n========== MODULE 8: COMBINED - OOP + Sorting ==========\n";
    ScoringList list;
    list.add("Aarav", 85);
    list.add("John", 78);
    list.add("Emma", 92);
    list.sort();
    list.display();
}
