// Module 2: Delegates Management - OOP Version (C++)
// Concepts: Inheritance, Virtual Methods, Polymorphism

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#include "../include/munsphere_modules.h"

using namespace std;

// Everything in this unnamed namespace is private to this file.
// Several modules use the same class names (Session, Delegate, Score...);
// without this, the linker merged different classes with the same name
// (One Definition Rule violation), which caused crashes.
namespace {

class Person {
protected:
    string id, name, country;
public:
    Person(string i, string n, string c) : id(i), name(n), country(c) {}
    virtual ~Person() {}
    virtual void display() const {
        cout << "Person: " << name << " (" << country << ")\n";
    }
    virtual string getRole() const { return "Person"; }
};

class Delegate : public Person {
private:
    string committee;
    int score;
public:
    Delegate(string i, string n, string c, string com, int s = 0)
        : Person(i, n, c), committee(com), score(s) {}
    
    void display() const override {
        cout << "[" << id << "] " << name << " | " << country 
             << " | " << committee << " | Score: " << score << "\n";
    }
    
    string getRole() const override { return "Delegate"; }
    
    void setScore(int s) { score = s; }
    int getScore() const { return score; }
};

class DelegateManager {
private:
    vector<Delegate> delegates;
public:
    void add(string i, string n, string c, string com, int s) {
        delegates.push_back(Delegate(i, n, c, com, s));
    }
    
    void displayAll() {
        cout << "\n=== DELEGATES ===\n";
        for (const auto& d : delegates) d.display();
    }
    
    void displayTopN(int n) {
        vector<Delegate> sorted = delegates;
        sort(sorted.begin(), sorted.end(), 
             [](const Delegate& a, const Delegate& b) {
                 return a.getScore() > b.getScore();
             });
        cout << "\n=== TOP " << n << " DELEGATES ===\n";
        for (int i = 0; i < min(n, (int)sorted.size()); i++) {
            sorted[i].display();
        }
    }
};

} // unnamed namespace

void module02_oop_demo() {
    cout << "\n========== MODULE 2: OOP - Delegates Inheritance ==========\n";
    
    DelegateManager manager;
    manager.add("D001", "Aarav Sharma", "India", "UNGA", 85);
    manager.add("D002", "John Smith", "USA", "UNHRC", 78);
    manager.add("D003", "Emma Wilson", "UK", "AIPPM", 92);
    
    manager.displayAll();
    manager.displayTopN(2);
}
