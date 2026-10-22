// Module 10: Executive Board - COMBINED (C++ class + Linked List)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

// One node of the linked list
class EBNode {
public:
    string name, position;
    EBNode* next;

    EBNode(string n, string p) {
        name = n;
        position = p;
        next = NULL;
    }
};

class ExecutiveBoardList {
private:
    EBNode* head;

public:
    ExecutiveBoardList() { head = NULL; }

    // destructor: free every node
    ~ExecutiveBoardList() {
        while (head != NULL) {
            EBNode* next = head->next;
            delete head;
            head = next;
        }
    }

    // insert at the end of the list
    void add(string name, string position) {
        EBNode* node = new EBNode(name, position);
        if (head == NULL) {
            head = node;
        } else {
            EBNode* curr = head;
            while (curr->next != NULL) curr = curr->next;
            curr->next = node;
        }
    }

    void display() {
        cout << "\n=== EXECUTIVE BOARD ===\n";
        EBNode* curr = head;
        while (curr != NULL) {
            cout << curr->name << " - " << curr->position << "\n";
            curr = curr->next;
        }
    }
};

}

void module10_combined_demo() {
    cout << "\n========== MODULE 10: COMBINED - OOP + Linked List ==========\n";
    ExecutiveBoardList board;
    board.add("Aarav Sharma", "Secretary General");
    board.add("Emma Wilson", "Under Secretary");
    board.display();
}
