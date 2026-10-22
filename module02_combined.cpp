// Module 2: Delegates - COMBINED (OOP Classes + Queue DS)

#include <iostream>
#include <string>
#include <cstring>

#include "../include/munsphere_modules.h"

using namespace std;


namespace {

#define MAX_QUEUE 100

struct Delegate {
    string id, name, country;
    int score;
};

class DelegateQueue {
private:
    Delegate items[MAX_QUEUE];
    int front, rear, size;
    
public:
    DelegateQueue() : front(0), rear(-1), size(0) {}
    
    void enqueue(string id, string name, string country, int score = 0) {
        if (size < MAX_QUEUE) {
            rear = (rear + 1) % MAX_QUEUE;
            items[rear] = {id, name, country, score};
            size++;
            cout << "Enqueued: " << name << "\n";
        }
    }
    
    void dequeue() {
        if (size > 0) {
            cout << "Dequeued: " << items[front].name << "\n";
            front = (front + 1) % MAX_QUEUE;
            size--;
        }
    }
    
    void display() {
        cout << "\n=== Delegates Queue ===\n";
        cout << left;
        cout.width(8); cout << "ID";
        cout.width(20); cout << "Name";
        cout.width(15); cout << "Country";
        cout.width(8); cout << "Score" << "\n";
        cout << "-------------------------------------------\n";
        
        for (int i = 0; i < size; i++) {
            int idx = (front + i) % MAX_QUEUE;
            cout.width(8); cout << items[idx].id;
            cout.width(20); cout << items[idx].name;
            cout.width(15); cout << items[idx].country;
            cout.width(8); cout << items[idx].score << "\n";
        }
        cout << "\nQueue Size: " << size << "\n";
    }
    
    int getSize() { return size; }
};

} // unnamed namespace

void module02_combined_demo() {
    cout << "\n========== MODULE 2: COMBINED - OOP + Queue ==========\n";
    
    DelegateQueue queue;
    
    queue.enqueue("D001", "Aarav Sharma", "India", 85);
    queue.enqueue("D002", "John Smith", "USA", 78);
    queue.enqueue("D003", "Emma Wilson", "UK", 92);
    queue.enqueue("D004", "Yuki Tanaka", "Japan", 88);
    
    queue.display();
    
    cout << "\nProcessing queue...\n";
    queue.dequeue();
    queue.dequeue();
    
    queue.display();
}
