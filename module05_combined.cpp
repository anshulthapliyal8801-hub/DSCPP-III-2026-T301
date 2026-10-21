// Module 5: Speakers - COMBINED (C++ class + Stack)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

#define MAX_SPK 50

struct SpeakerData {
    string name;
    int time;
};

class SpeakerStack {
private:
    SpeakerData items[MAX_SPK];
    int top;

public:
    SpeakerStack() { top = -1; }

    void push(string name, int time) {
        if (top < MAX_SPK - 1) {
            top++;
            items[top].name = name;
            items[top].time = time;
        }
    }

    void display() {
        cout << "\n=== SPEAKERS (Stack) ===\n";
        for (int i = top; i >= 0; i--) {
            cout << items[i].name << " - " << items[i].time << "s\n";
        }
    }
};

}

void module05_combined_demo() {
    cout << "\n========== MODULE 5: COMBINED - OOP + Stack ==========\n";
    SpeakerStack stack;
    stack.push("Aarav", 300);
    stack.push("John", 250);
    stack.push("Emma", 280);
    stack.display();
}
