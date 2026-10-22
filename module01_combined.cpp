

#include <iostream>
#include <vector>
#include <string>
#include <cstring>

#include "../include/munsphere_modules.h"

using namespace std;

// Everything in this unnamed namespace is private to this file.
// Several modules use the same class names (Session, Delegate, Score...);
// without this, the linker merged different classes with the same name
// (One Definition Rule violation), which caused crashes.
namespace {

#define MAX_HISTORY 50

struct HistoryEntry {
    char action[200];
};

struct HistoryStack {
    HistoryEntry items[MAX_HISTORY];
    int top;
    
    HistoryStack() : top(-1) {}
    
    void push(const string& action) {
        if (top < MAX_HISTORY - 1) {
            strcpy(items[++top].action, action.c_str());
        }
    }
    
    void pop() {
        if (top >= 0) top--;
    }
    
    string peek() {
        if (top >= 0) return items[top].action;
        return "No history";
    }
    
    void display() {
        cout << "\n--- Action History ---\n";
        for (int i = top; i >= 0; i--) {
            cout << "[" << (i + 1) << "] " << items[i].action << "\n";
        }
    }
};

class Session {
private:
    string sessionId, name, attendees;
    HistoryStack history;
    
public:
    Session(string id, string n) : sessionId(id), name(n), attendees("0") {
        history.push("Session created: " + name);
    }
    
    void setAttendees(string count) {
        attendees = count;
        history.push("Attendees updated to: " + count);
    }
    
    void display() {
        cout << "Session: " << name << " (" << attendees << " attendees)\n";
    }
    
    HistoryStack& getHistory() { return history; }
};

class Conference {
private:
    string name;
    vector<Session> sessions;
    HistoryStack conferenceHistory;
    
public:
    Conference(string n) : name(n) {
        conferenceHistory.push("Conference created: " + name);
    }
    
    void addSession(string id, string sessionName) {
        sessions.push_back(Session(id, sessionName));
        conferenceHistory.push("Session added: " + sessionName);
    }
    
    void displayAll() {
        cout << "\n=== COMBINED: " << name << " ===\n";
        cout << "Sessions (" << sessions.size() << "):\n";
        for (auto& s : sessions) s.display();
        conferenceHistory.display();
    }
};

} // unnamed namespace

void module01_combined_demo() {
    cout << "\n========== MODULE 1: COMBINED - OOP Classes + Stack History ==========\n";
    
    Conference conference("MUNSphere 2026");
    
    conference.addSession("S001", "Opening Ceremony");
    conference.addSession("S002", "General Debate");
    conference.addSession("S003", "Formal Voting");
    
    conference.displayAll();
}
