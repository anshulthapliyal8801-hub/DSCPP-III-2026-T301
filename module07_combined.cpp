// Module 7: Voting - COMBINED (C++ class + Array)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

struct VoteRecord {
    string delegateId, choice;
};

class VotingSystem {
private:
    VoteRecord votes[200];
    int count;

public:
    VotingSystem() { count = 0; }

    void vote(string id, string choice) {
        if (count < 200) {
            votes[count].delegateId = id;
            votes[count].choice = choice;
            count++;
        }
    }

    void display() {
        cout << "\n=== VOTING RESULTS ===\n";
        int for_cnt = 0, against_cnt = 0;
        for (int i = 0; i < count; i++) {
            cout << votes[i].delegateId << ": " << votes[i].choice << "\n";
            if (votes[i].choice == "For") {
                for_cnt++;
            } else {
                against_cnt++;
            }
        }
        cout << "\nFor: " << for_cnt << " | Against: " << against_cnt << "\n";
    }
};

}

void module07_combined_demo() {
    cout << "\n========== MODULE 7: COMBINED - OOP + Array ==========\n";
    VotingSystem voting;
    voting.vote("D001", "For");
    voting.vote("D002", "For");
    voting.vote("D003", "Against");
    voting.display();
}
