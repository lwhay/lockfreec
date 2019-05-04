#pragma once

#include <iostream>
#include <random>
#include <chrono>
#include <sys/time.h>
//#include <unistd.h>
#include <math.h>

#define EPS 0.0001f

#define FUZZY_BOUND 100

using namespace std;

class Tracer {
    timeval begTime;

    timeval endTime;

    long duration;

public:
    void startTime() {
        gettimeofday(&begTime, nullptr);
    }

    long getRunTime() {
        gettimeofday(&endTime, nullptr);
        //cout << endTime.tv_sec << "<->" << endTime.tv_usec << "\t";
        duration = (endTime.tv_sec - begTime.tv_sec) * 1000000 + endTime.tv_usec - begTime.tv_usec;
        begTime = endTime;
        return duration;
    }

    long fetchTime() {
        cout << endTime.tv_sec << " " << endTime.tv_usec << endl;
        return duration;
    }
};

template<typename R>
class UniformGen {
public:
    static inline void generate(R *array, size_t count) {
        std::default_random_engine engine(static_cast<R>(chrono::steady_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<size_t> dis(0, count + FUZZY_BOUND);
        for (size_t i = 0; i < count; i++) {
            array[i] = static_cast<R>(dis(engine));
        }
    }
};