//
// Created by Michael on 2019-05-03.
//

#include <iostream>
#include <sstream>
#include "TMHashMap.h"
#include "OneFilePTMWF.h"

#define TOTAL_COUNT   (1 << 20)

#define QUERY_COUNT   (1 << 7)

#define THREAD_NUBMER 4

#define TEST_LOOKUP   1

long total_time;

uint64_t exists = 0;

uint64_t update = 0;

uint64_t failure = 0;

uint64_t total_count = TOTAL_COUNT;

int thread_number = THREAD_NUBMER;

TMHashMap<uint64_t, uint64_t, pofwf::OneFileWF, pofwf::tmtype> *set;

stringstream *output;

struct target {
    int tid;
    TMHashMap<uint64_t, uint64_t, pofwf::OneFileWF, pofwf::tmtype> *set;
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
        if (!work->set->add(i)) {
            fail++;
        }
    }
    __sync_fetch_and_add(&exists, fail);
}

void *measureWorker(void *args) {
    Tracer tracer;
    tracer.startTime();
    struct target *work = (struct target *) args;
    //cout << "Updater " << work->tid << endl;
    uint64_t hit = 0;
    uint64_t fail = 0;
    for (int i = work->tid; i < total_count; i += thread_number) {
#if TEST_LOOKUP
        if (work->set->contains(i)) {
            hit++;
        } else {
            fail++;
        }
#else
        if (work->set->remove(i)) {
            hit++;
            if (!work->set->add(i)) {
                fail++;
            }
        }
#endif
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
    cout << "Measuring ..." << endl;
    for (int i = 0; i < thread_number; i++) {
        pthread_create(&workers[i], nullptr, measureWorker, &parms[i]);
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
    pofwf::OneFileWF::updateTx([&]() {
        set = pofwf::OneFileWF::tmNew<TMHashMap<uint64_t, uint64_t, pofwf::OneFileWF, pofwf::tmtype>>(1000);
    });
    Tracer tracer;
    tracer.startTime();
    simpleInsert();
    long it = tracer.getRunTime();
    tracer.startTime();
    multiWorkers();
    long ut = tracer.getRunTime();
    cout << "IT " << it << " ut " << ut << " dupinst " << exists << " tryupd " << update << " failinst " << failure
         << " avgtpt " << (double) update * 1000000 * thread_number / total_time << endl;
    pofwf::OneFileWF::updateTx([&]() { pofwf::OneFileWF::tmDelete(set); });
    delete[] output;
}