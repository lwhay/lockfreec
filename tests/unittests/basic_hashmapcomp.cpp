//
// Created by Michael on 2019/6/8.
//

#include <unordered_map>
#include <map>
#include <iostream>
#include "tracer.h"

using namespace std;

template<typename T>
void unordered_map_func() {
    Tracer tracer;
    for (long p = 16; p <= 26; p++) {
        unordered_map<T, T> map;
        tracer.startTime();
        for (int i = 0; i < (1 << p); i++) {
            map.insert(make_pair(static_cast<T>(i), static_cast<T>(i)));
        }
        long elipsed = tracer.getRunTime();
        cout << "Uod Map: " << (1 << p) << " time: " << elipsed << " tpt: " << (double) (1 << p) / elipsed << endl;
    }
}

template<typename T>
void std_map_func() {
    Tracer tracer;
    for (long p = 16; p <= 26; p++) {
        map<T, T> map;
        tracer.startTime();
        for (int i = 0; i < (1 << p); i++) {
            map.insert(make_pair(static_cast<T>(i), static_cast<T>(i)));
        }
        long elipsed = tracer.getRunTime();
        cout << "Std Map: " << (1 << p) << " time: " << elipsed << " tpt: " << (double) (1 << p) / elipsed << endl;
    }
}

int main(int argc, char **argv) {
    cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@ int @@@@@@@@@@@@@@@@@@@@@@@@@@@" << endl;
    for (int r = 0; r < 5; r++) {
        unordered_map_func<int>();
        std_map_func<int>();
        cout << "*****************************************************" << endl;
    }
    cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@ long @@@@@@@@@@@@@@@@@@@@@@@@@@" << endl;
    for (int r = 0; r < 5; r++) {
        unordered_map_func<long>();
        std_map_func<long>();
        cout << "*****************************************************" << endl;
    }
    cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@ double @@@@@@@@@@@@@@@@@@@@@@@@" << endl;
    for (int r = 0; r < 5; r++) {
        unordered_map_func<double>();
        std_map_func<double>();
        cout << "*****************************************************" << endl;
    }
    return 0;
}