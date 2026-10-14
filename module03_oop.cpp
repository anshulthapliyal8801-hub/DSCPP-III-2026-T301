// Module 3: Committees - OOP Version
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

class Committee {
private:
    string id, name;
    int capacity, members;
public:
    Committee(string i, string n, int c) : id(i), name(n), capacity(c), members(0) {}
    void addMember() { if (members < capacity) members++; }
    void display() { cout << "[" << id << "] " << name << ": " << members << "/" << capacity << "\n"; }
};

class CommitteeManager {
private:
    vector<Committee> committees;
public:
    void add(string i, string n, int c) { committees.push_back(Committee(i, n, c)); }
    void displayAll() {
        cout << "\n=== COMMITTEES ===\n";
        for (auto& c : committees) c.display();
    }
};

} // unnamed namespace

void module03_oop_demo() {
    cout << "\n========== MODULE 3: OOP - Committees ==========\n";
    CommitteeManager mgr;
    mgr.add("COM1", "UNGA", 20);
    mgr.add("COM2", "UNHRC", 15);
    mgr.add("COM3", "AIPPM", 18);
    mgr.displayAll();
}
