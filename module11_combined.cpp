// Module 11: Reports - COMBINED (C++ class + Queue)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

struct ReportItem {
    string title, status;
};

class ReportQueue {
private:
    ReportItem items[100];
    int front, rear, size;

public:
    ReportQueue() {
        front = 0;
        rear = -1;
        size = 0;
    }

    void enqueue(string title) {
        if (size < 100) {
            rear = (rear + 1) % 100;
            items[rear].title = title;
            items[rear].status = "Pending";
            size++;
        }
    }

    void display() {
        cout << "\n=== REPORT QUEUE ===\n";
        for (int i = 0; i < size; i++) {
            int idx = (front + i) % 100;
            cout << (i + 1) << ". " << items[idx].title << " - "
                 << items[idx].status << "\n";
        }
    }
};

}

void module11_combined_demo() {
    cout << "\n========== MODULE 11: COMBINED - OOP + Queue ==========\n";
    ReportQueue queue;
    queue.enqueue("Attendance Report");
    queue.enqueue("Voting Results");
    queue.display();
}
