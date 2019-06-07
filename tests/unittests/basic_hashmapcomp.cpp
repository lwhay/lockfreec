//
// Created by Michael on 2019/6/8.
//

#include <unordered_map>
#include <map>
#include <iostream>
#include "tracer.h"

using namespace std;

void unordered_map_func() {
    Tracer tracer;
    tracer.startTime();
    unordered_map<int, int> map;
    for (int i = 0; i < (1 << 22); i++) {
        map.insert(make_pair(i, i));
    }
    cout << "Unordered Map: " << tracer.getRunTime() << endl;
}

void std_map_func() {
    Tracer tracer;
    tracer.startTime();
    map<int, int> map;
    for (int i = 0; i < (1 << 22); i++) {
        map.insert(make_pair(i, i));
    }
    cout << "Unordered Map: " << tracer.getRunTime() << endl;
}

int main(int argc, char **argv) {
    unordered_map_func();
    std_map_func();
    return 0;
}