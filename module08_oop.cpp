// Module 8: Scoring - OOP Version (C++)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

class ScoreEntry {
private:
    string delegateId, name;
    int score;

public:
    ScoreEntry(string id, string n, int s) {
        delegateId = id;
        name = n;
        score = s;
    }

    void display() {
        cout << name << " (" << delegateId << "): " << score << " pts\n";
    }

    int getScore() const { return score; }
};

class ScoringManager {
private:
    ScoreEntry* scores[100];
    int count;

public:
    ScoringManager() { count = 0; }

    // destructor: delete the objects created with new
    ~ScoringManager() {
        for (int i = 0; i < count; i++) {
            delete scores[i];
        }
    }

    void add(string id, string name, int score) {
        if (count < 100) {
            scores[count] = new ScoreEntry(id, name, score);
            count++;
        }
    }

    void display() {
        cout << "\n=== SCORES ===\n";
        for (int i = 0; i < count; i++) {
            scores[i]->display();
        }
    }
};

}

void module08_oop_demo() {
    cout << "\n========== MODULE 8: OOP - Scoring ==========\n";
    ScoringManager mgr;
    mgr.add("D001", "Aarav", 85);
    mgr.add("D002", "John", 78);
    mgr.add("D003", "Emma", 92);
    mgr.display();
}
