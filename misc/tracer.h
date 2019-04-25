#pragma once

#include <iostream>
#include <sys/time.h>
//#include <unistd.h>
#include <math.h>

#define EPS 0.0001f

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
