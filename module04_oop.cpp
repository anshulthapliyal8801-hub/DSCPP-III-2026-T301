// Module 4: Attendance - OOP
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

class AttendanceRecord {
private:
    string delegateId, name;
    bool present;
public:
    AttendanceRecord(string id, string n, bool p) : delegateId(id), name(n), present(p) {}
    void display() { cout << name << ": " << (present ? "Present" : "Absent") << "\n"; }
};

class AttendanceManager {
private:
    vector<AttendanceRecord> records;
public:
    void mark(string id, string name, bool present) {
        records.push_back(AttendanceRecord(id, name, present));
    }
    void display() {
        cout << "\n=== ATTENDANCE ===\n";
        for (auto& r : records) r.display();
    }
};

} // unnamed namespace

void module04_oop_demo() {
    cout << "\n========== MODULE 4: OOP - Attendance ==========\n";
    AttendanceManager mgr;
    mgr.mark("D001", "Aarav", true);
    mgr.mark("D002", "John", true);
    mgr.mark("D003", "Emma", false);
    mgr.display();
}
