//
// Created by lwh on 19-5-6.
//

#include <iostream>
#include <sstream>
#include "tracer.h"
#include "OneFileLF.h"

#define TOTAL_COUNT   (1 << 20)

#define QUERY_COUNT   (1 << 7)

#define THREAD_NUBMER 8

long total_time;

uint64_t exists = 0;

uint64_t update = 0;

uint64_t failure = 0;

uint64_t total_count = TOTAL_COUNT;

int thread_number = THREAD_NUBMER;

uint64_t *loads;

oflf::tmtype<uint64_t> value;

uint64_t ralue;

stringstream *output;

struct target {
    int tid;
    oflf::tmtype<uint64_t> value;
};

void *measureWorker(void *args) {
    Tracer tracer;
    tracer.startTime();
    struct target *work = (struct target *) args;
    uint64_t hit = 0;
    uint64_t fail = 0;
    for (int i = work->tid; i < total_count; i++) {
        work->value = loads[i];
        hit++;
    }
    long elipsed = tracer.getRunTime();
    output[work->tid] << work->tid << " " << elipsed << " " << endl;
    __sync_fetch_and_add(&total_time, elipsed);
    __sync_fetch_and_add(&update, hit);
    __sync_fetch_and_add(&failure, fail);
}

void multiWorkers() {
    pthread_t workers[thread_number];
    struct target *parms = (struct target *) oflf::tmMalloc(sizeof(struct target) * thread_number);
    cout << "Measuring ..." << endl;
    oflf::gOFLF.debug = true;
    for (int i = 0; i < thread_number; i++) {
        parms[i].tid = i;
        parms[i].value = value;
        pthread_create(&workers[i], nullptr, measureWorker, &parms[i]);
    }
    for (int i = 0; i < thread_number; i++) {
        pthread_join(workers[i], nullptr);
        string outstr = output[i].str();
        cout << outstr;
    }
    oflf::gOFLF.debug = false;
    cout << "Gathering ..." << endl;
    oflf::tmFree(parms);
}

int main(int argc, char **argv) {
    if (argc == 3) {
        cout << "Parameters " << argc << endl;
        thread_number = atoi(argv[1]);
        total_count = atoi(argv[2]);
    }
    output = new stringstream[thread_number];
    loads = new uint64_t[total_count];
    for (int i = 0; i < total_count; i++) {
        loads[i] = i;
    }
    Tracer tracer;
    tracer.startTime();
    multiWorkers();
    long ut = tracer.getRunTime();
    cout << "ut " << ut << " dupinst " << exists << " tryupd " << update << " failinst " << failure << " avgtpt "
         << (double) update * 1000000 * thread_number / total_time << endl;
    delete[] loads;
    delete[] output;
}