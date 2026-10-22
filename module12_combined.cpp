// Module 12: Persistence & Audit - COMBINED (C++ class + Stack)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

#define MAX_LOG 500

struct LogData {
    string timestamp, action;
};

class AuditLogStack {
private:
    LogData items[MAX_LOG];
    int top;

public:
    AuditLogStack() { top = -1; }

    void log(string timestamp, string action) {
        if (top < MAX_LOG - 1) {
            top++;
            items[top].timestamp = timestamp;
            items[top].action = action;
        }
    }

    void display() {
        cout << "\n=== AUDIT LOG (Most Recent First) ===\n";
        for (int i = top; i >= 0; i--) {
            cout << "[" << items[i].timestamp << "] " << items[i].action << "\n";
        }
    }
};

}

void module12_combined_demo() {
    cout << "\n========== MODULE 12: COMBINED - OOP + Stack Logging ==========\n";
    AuditLogStack logs;
    logs.log("2026-10-15 09:00", "Conference Created");
    logs.log("2026-10-15 09:15", "Session Added");
    logs.log("2026-10-16 10:00", "Voting Started");
    logs.display();
}
