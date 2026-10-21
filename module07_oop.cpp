// Module 7: Voting - OOP Version (C++)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

class Vote {
private:
    string delegateId, choice;

public:
    Vote(string id, string c) {
        delegateId = id;
        choice = c;
    }

    void display() {
        cout << "Delegate " << delegateId << ": " << choice << "\n";
    }
};

class VotingManager {
private:
    Vote* votes[100];
    int count;

public:
    VotingManager() { count = 0; }

    // destructor: delete the objects created with new
    ~VotingManager() {
        for (int i = 0; i < count; i++) {
            delete votes[i];
        }
    }

    void vote(string id, string choice) {
        if (count < 100) {
            votes[count] = new Vote(id, choice);
            count++;
        }
    }

    void display() {
        cout << "\n=== VOTES ===\n";
        for (int i = 0; i < count; i++) {
            votes[i]->display();
        }
        cout << "Total Votes: " << count << "\n";
    }
};

}

void module07_oop_demo() {
    cout << "\n========== MODULE 7: OOP - Voting ==========\n";
    VotingManager mgr;
    mgr.vote("D001", "For");
    mgr.vote("D002", "For");
    mgr.vote("D003", "Against");
    mgr.display();
}
