// Module 3: Committees - COMBINED (OOP + Linked List)
#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// Everything in this unnamed namespace is private to this file.
// Several modules use the same class names (Session, Delegate, Score...);
// without this, the linker merged different classes with the same name
// (One Definition Rule violation), which caused crashes.
namespace {

class CommitteeNode {
public:
    string id, name;
    int capacity, members;
    CommitteeNode* next;
    
    CommitteeNode(string i, string n, int c) 
        : id(i), name(n), capacity(c), members(0), next(nullptr) {}
    
    void display() {
        cout << "[" << id << "] " << name << ": " << members << "/" << capacity << "\n";
    }
};

class CommitteeList {
private:
    CommitteeNode* head;
public:
    CommitteeList() : head(nullptr) {}
    // Destructor frees every node (fixes the memory leak)
    ~CommitteeList() {
        while (head) {
            CommitteeNode* next = head->next;
            delete head;
            head = next;
        }
    }
    // Copying would make two lists share (and double-delete) the same nodes
    CommitteeList(const CommitteeList&) = delete;
    CommitteeList& operator=(const CommitteeList&) = delete;
    
    void add(string i, string n, int c) {
        CommitteeNode* node = new CommitteeNode(i, n, c);
        if (!head) head = node;
        else {
            CommitteeNode* curr = head;
            while (curr->next) curr = curr->next;
            curr->next = node;
        }
    }
    
    void display() {
        cout << "\n=== COMMITTEES (Linked List) ===\n";
        for (CommitteeNode* curr = head; curr; curr = curr->next) {
            curr->display();
        }
    }
};

} // unnamed namespace

void module03_combined_demo() {
    cout << "\n========== MODULE 3: COMBINED - OOP + Linked List ==========\n";
    CommitteeList list;
    list.add("COM1", "UNGA", 20);
    list.add("COM2", "UNHRC", 15);
    list.add("COM3", "AIPPM", 18);
    list.display();
}
