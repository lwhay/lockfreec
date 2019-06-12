//
// Created by lwh on 19-6-5.
//


#include <iostream>
#include <sstream>
#include "LFResizableHashSet.h"

#define TOTAL_COUNT   (1 << 20)

#define MALLOC_GRAN   (1 << 7)

#define THREAD_NUBMER 4

#define TEST_LOOKUP   1

#define RANDOM_KEYS   1

long malloc_time;

long free_time;

uint64_t malloc_count = 0;

uint64_t free_count = 0;

uint64_t total_count = TOTAL_COUNT;

int thread_number = THREAD_NUBMER;

stringstream *output;

atomic<int> stopMeasure(0);

struct target {
    int tid;
    char **mm;
};

void *measureWorker(void *args) {
    Tracer tracer;
    struct target *work = (struct target *) args;
    uint64_t hit = 0;
    uint64_t fail = 0;
    long malloc_elipsed = 0;
    long free_elipsed = 0;
    while (stopMeasure.load(memory_order_relaxed) == 0) {
        tracer.startTime();
        for (int i = 0; i < total_count / thread_number; i++) {
            work->mm[i] = (char *) malloc(MALLOC_GRAN);
            hit++;
        }
        malloc_elipsed += tracer.getRunTime();
        tracer.startTime();
        for (int i = 0; i < total_count / thread_number; i++) {
            free(work->mm[i]);
            fail++;
        }
        free_elipsed += tracer.getRunTime();
    }
    output[work->tid] << work->tid << " " << malloc_elipsed << " " << free_elipsed << " "
                      << malloc_elipsed + free_elipsed << endl;

    __sync_fetch_and_add(&malloc_time, malloc_elipsed);
    __sync_fetch_and_add(&free_time, free_elipsed);
    __sync_fetch_and_add(&malloc_count, hit);
    __sync_fetch_and_add(&free_count, fail);
}

void multiWorkers() {
    pthread_t workers[thread_number];
    struct target parms[thread_number];
    cout << "Measuring count: " << total_count << " thread: " << thread_number << " gran: " << MALLOC_GRAN << endl;
    Timer timer;
    timer.start();
    for (int i = 0; i < thread_number; i++) {
        parms[i].tid = i;
        parms[i].mm = (char **) malloc(total_count / thread_number * sizeof(char *));
        pthread_create(&workers[i], nullptr, measureWorker, &parms[i]);
    }
    while (timer.elapsedSeconds() < default_timer_range) {
        sleep(1);
    }
    stopMeasure.store(1, memory_order_relaxed);
    for (int i = 0; i < thread_number; i++) {
        pthread_join(workers[i], nullptr);
        free(parms[i].mm);
        string outstr = output[i].str();
        cout << outstr;
    }
    cout << "Gathering ..." << endl;
}

int main(int argc, char **argv) {
    if (argc == 3) {
        cout << "Parameters " << argc << endl;
        thread_number = atoi(argv[1]);
        total_count = atoi(argv[2]);
    }
    output = new stringstream[thread_number];
    Tracer tracer;
    tracer.startTime();
    multiWorkers();
    long ut = tracer.getRunTime();
    cout << " Total " << ut << " malloc count " << malloc_count << " free count " << free_count
         << " malloc avgtpt " << (double) malloc_count * 1000000 * thread_number / malloc_time
         << " free avgtpt " << (double) free_count * 1000000 * thread_number / free_time
         << " total avgtpt " << (double) (malloc_count) * 1000000 * thread_number / (malloc_time + free_time) << endl;
    delete[] output;
}