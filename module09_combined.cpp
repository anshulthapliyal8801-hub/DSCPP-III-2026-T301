// Module 9: Awards - COMBINED (C++ class + Stack)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

#define MAX_AWD 50

struct AwardData {
    string name, type;
};

class AwardStack {
private:
    AwardData items[MAX_AWD];
    int top;

public:
    AwardStack() { top = -1; }

    void push(string name, string type) {
        if (top < MAX_AWD - 1) {
            top++;
            items[top].name = name;
            items[top].type = type;
        }
    }

    void display() {
        cout << "\n=== AWARDS ===\n";
        for (int i = top; i >= 0; i--) {
            cout << "🏆 " << items[i].name << " - " << items[i].type << "\n";
        }
    }
};

}

void module09_combined_demo() {
    cout << "\n========== MODULE 9: COMBINED - OOP + Stack ==========\n";
    AwardStack stack;
    stack.push("Aarav", "Gold");
    stack.push("Emma", "Gold");
    stack.push("John", "Silver");
    stack.display();
}
