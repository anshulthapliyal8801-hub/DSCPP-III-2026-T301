// Module 6: Motions - OOP Version (C++)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

class Motion {
private:
    string id, title, status;

public:
    Motion(string i, string t) {
        id = i;
        title = t;
        status = "Proposed";
    }

    void setStatus(string s) { status = s; }

    void display() {
        cout << "[" << id << "] " << title << " - " << status << "\n";
    }
};

class MotionManager {
private:
    Motion* motions[100];
    int count;

public:
    MotionManager() { count = 0; }

    // destructor: delete the objects created with new
    ~MotionManager() {
        for (int i = 0; i < count; i++) {
            delete motions[i];
        }
    }

    void add(string i, string t) {
        if (count < 100) {
            motions[count] = new Motion(i, t);
            count++;
        }
    }

    void display() {
        cout << "\n=== MOTIONS ===\n";
        for (int i = 0; i < count; i++) {
            motions[i]->display();
        }
    }
};

}

void module06_oop_demo() {
    cout << "\n========== MODULE 6: OOP - Motions ==========\n";
    MotionManager mgr;
    mgr.add("M1", "Climate Action");
    mgr.add("M2", "Education Reform");
    mgr.display();
}
