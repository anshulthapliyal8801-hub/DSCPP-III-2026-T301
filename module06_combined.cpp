
#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;


namespace {

// One node of the linked list
class MotionNode {
public:
    string id, title, status;
    MotionNode* next;

    MotionNode(string i, string t) {
        id = i;
        title = t;
        status = "Proposed";
        next = NULL;
    }

    void display() {
        cout << "[" << id << "] " << title << " - " << status << "\n";
    }
};

class MotionList {
private:
    MotionNode* head;

public:
    MotionList() { head = NULL; }

    // destructor: free every node
    ~MotionList() {
        while (head != NULL) {
            MotionNode* next = head->next;
            delete head;
            head = next;
        }
    }

    // insert at the end of the list
    void add(string i, string t) {
        MotionNode* node = new MotionNode(i, t);
        if (head == NULL) {
            head = node;
        } else {
            MotionNode* curr = head;
            while (curr->next != NULL) curr = curr->next;
            curr->next = node;
        }
    }

    void display() {
        cout << "\n=== MOTIONS ===\n";
        MotionNode* curr = head;
        while (curr != NULL) {
            curr->display();
            curr = curr->next;
        }
    }
};

}

void module06_combined_demo() {
    cout << "\n========== MODULE 6: COMBINED - OOP + Linked List ==========\n";
    MotionList list;
    list.add("M1", "Climate Action");
    list.add("M2", "Education Reform");
    list.display();
}
