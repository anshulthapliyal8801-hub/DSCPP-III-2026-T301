// Module 1: Conference Management - OOP Version (C++)
// Concepts: Classes, Encapsulation, Inheritance, Polymorphism

#include <iostream>
#include <vector>
#include <string>

#include "../include/munsphere_modules.h"

using namespace std;

// Everything in this unnamed namespace is private to this file.
// Several modules use the same class names (Session, Delegate, Score...);
// without this, the linker merged different classes with the same name
// (One Definition Rule violation), which caused crashes.
namespace {

class Session {
private:
    string sessionId, name, date, time, location, status;
    int capacity, attendees;

public:
    Session(string id, string n, string d, string t, string loc) 
        : sessionId(id), name(n), date(d), time(t), location(loc), 
          status("Scheduled"), capacity(200), attendees(0) {}
    
    string getName() const { return name; }
    int getAttendees() const { return attendees; }
    int getCapacity() const { return capacity; }
    
    void setAttendees(int a) { attendees = a; }
    
    void display() const {
        cout << "  [" << sessionId << "] " << name << " | " 
             << attendees << "/" << capacity << " attendees\n";
    }
};

class Conference {
private:
    string conferenceId, name, location;
    vector<Session> sessions;
    
public:
    Conference(string id, string n, string loc)
        : conferenceId(id), name(n), location(loc) {}
    
    void addSession(string id, string n, string d, string t, string loc) {
        sessions.push_back(Session(id, n, d, t, loc));
    }
    
    void displayInfo() {
        cout << "\n=== CONFERENCE: " << name << " ===" << endl;
        cout << "Location: " << location << endl;
        cout << "Total Sessions: " << sessions.size() << "\n";
        for (const auto& s : sessions) s.display();
    }
};

} // unnamed namespace

void module01_oop_demo() {
    cout << "\n========== MODULE 1: OOP - Conference Management ==========\n";
    
    Conference conf("CONF001", "MUNSphere 2026", "New Delhi");
    conf.addSession("S1", "Opening", "2026-10-15", "09:00", "Main Hall");
    conf.addSession("S2", "Debate", "2026-10-15", "14:00", "Committee");
    conf.addSession("S3", "Voting", "2026-10-16", "10:00", "Main Hall");
    
    conf.displayInfo();
}
