// Module 4: Attendance - COMBINED
#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// Everything in this unnamed namespace is private to this file.
// Several modules use the same class names (Session, Delegate, Score...);
// without this, the linker merged different classes with the same name
// (One Definition Rule violation), which caused crashes.
namespace {

class AttendanceData {
public:
    string name;
    bool present;
    AttendanceData(string n, bool p) : name(n), present(p) {}
};

class Attendance {
private:
    AttendanceData* records[100];
    int count;
public:
    Attendance() : count(0) {}
    // Destructor deletes the objects created with new (fixes the memory leak)
    ~Attendance() {
        for (int i = 0; i < count; i++) delete records[i];
    }
    Attendance(const Attendance&) = delete;
    Attendance& operator=(const Attendance&) = delete;
    void mark(string name, bool p) {
        if (count < 100) records[count++] = new AttendanceData(name, p);
    }
    void display() {
        cout << "\n=== ATTENDANCE ===\n";
        for (int i = 0; i < count; i++) {
            cout << records[i]->name << ": " << (records[i]->present ? "Present" : "Absent") << "\n";
        }
    }
};

} // unnamed namespace

void module04_combined_demo() {
    cout << "\n========== MODULE 4: COMBINED - OOP + Array ==========\n";
    Attendance att;
    att.mark("Aarav", true);
    att.mark("John", true);
    att.mark("Emma", false);
    att.display();
}
