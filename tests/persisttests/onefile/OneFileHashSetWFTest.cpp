//
// Created by Michael on 2019-04-25.
//

#include <iostream>
#include <sstream>
#include "WFResizableHashSet.h"

#define TOTAL_COUNT   (1 << 20)

#define QUERY_COUNT   (1 << 7)

#define THREAD_NUBMER 4

long total_time;

uint64_t exists = 0;

uint64_t update = 0;

uint64_t failure = 0;

uint64_t total_count = TOTAL_COUNT;

int thread_number = THREAD_NUBMER;

OFWFResizableHashSet<uint64_t> *set;

stringstream *output;

struct target {
    int tid;
    OFWFResizableHashSet<uint64_t> *set;
};

void simpleInsert() {
    for (int i = 0; i < total_count; i++) {
        set->add(i);
    }
    for (int i = 0; i < total_count; i += (total_count / QUERY_COUNT)) {
        cout << set->contains(i);
        if ((i + 1) / (total_count / QUERY_COUNT) % 128 == 0) {
            cout << endl;
        }
    }
}

void *insertWorker(void *args) {
    struct target *work = (struct target *) args;
    uint64_t fail = 0;
    for (int i = work->tid; i < total_count; i += thread_number) {
        if (!work->set->add(i, work->tid)) {
            fail++;
        }
    }
    __sync_fetch_and_add(&exists, fail);
}

void *updateWorker(void *args) {
    Tracer tracer;
    tracer.startTime();
    struct target *work = (struct target *) args;
    //cout << "Updater " << work->tid << endl;
    uint64_t hit = 0;
    uint64_t fail = 0;
    for (int i = work->tid; i < total_count; i += thread_number) {
        if (work->set->remove(i, work->tid)) {
            hit++;
            if (!work->set->add(i, work->tid)) {
                fail++;
            }
        }
    }
    long elipsed = tracer.getRunTime();
    output[work->tid] << work->tid << " " << elipsed << endl;
    __sync_fetch_and_add(&total_time, elipsed);
    __sync_fetch_and_add(&update, hit);
    __sync_fetch_and_add(&failure, fail);
}

void multiWorkers() {
    pthread_t workers[thread_number];
    struct target parms[thread_number];
    cout << endl << "Re-inserting ..." << endl;
    for (int i = 0; i < thread_number; i++) {
        parms[i].tid = i;
        parms[i].set = set;
        pthread_create(&workers[i], nullptr, insertWorker, &parms[i]);
    }
    for (int i = 0; i < thread_number; i++) {
        pthread_join(workers[i], nullptr);
    }
    cout << "Updating ..." << endl;
    for (int i = 0; i < thread_number; i++) {
        pthread_create(&workers[i], nullptr, updateWorker, &parms[i]);
    }
    for (int i = 0; i < thread_number; i++) {
        pthread_join(workers[i], nullptr);
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
    set = new OFWFResizableHashSet<uint64_t>(thread_number);
    Tracer tracer;
    tracer.startTime();
    simpleInsert();
    long it = tracer.getRunTime();
    tracer.startTime();
    multiWorkers();
    long ut = tracer.getRunTime();
    cout << "IT " << it << " ut " << ut << " dupinst " << exists << " tryupd " << update << " failinst " << failure
         << " avgtpt " << (double) total_count * 1000000 * thread_number / total_time << endl;
    for (int i = 0; i < total_count; i++) {
        set->remove(i);
    }
    delete set;
    delete[] output;
}