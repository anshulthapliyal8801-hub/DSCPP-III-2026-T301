// Module 5: Speakers - OOP Version (C++)

#include <iostream>
#include <string>
#include "../include/munsphere_modules.h"

using namespace std;

// The unnamed namespace keeps these class names private to this file,
// because other modules use the same names (Session, Delegate, Score...).
namespace {

class Speaker {
private:
    string id, name;
    int speakingTime;

public:
    Speaker(string i, string n) {
        id = i;
        name = n;
        speakingTime = 0;
    }

    void addTime(int t) { speakingTime += t; }

    void display() {
        cout << name << " - Speaking Time: " << speakingTime << "s\n";
    }
};

class SpeakerManager {
private:
    Speaker* speakers[100];
    int count;

public:
    SpeakerManager() { count = 0; }

    // destructor: delete the objects created with new
    ~SpeakerManager() {
        for (int i = 0; i < count; i++) {
            delete speakers[i];
        }
    }

    void add(string i, string n) {
        if (count < 100) {
            speakers[count] = new Speaker(i, n);
            count++;
        }
    }

    void display() {
        cout << "\n=== SPEAKERS ===\n";
        for (int i = 0; i < count; i++) {
            speakers[i]->display();
        }
    }
};

}

void module05_oop_demo() {
    cout << "\n========== MODULE 5: OOP - Speakers ==========\n";
    SpeakerManager mgr;
    mgr.add("S1", "Aarav");
    mgr.add("S2", "John");
    mgr.add("S3", "Emma");
    mgr.display();
}
