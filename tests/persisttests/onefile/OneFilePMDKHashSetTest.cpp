//
// Created by Michael on 2019-05-03.
//

#include <iostream>
#include <sstream>
#include "TMHashMapByRef.h"
#include "PMDKTM.h"

#define TOTAL_COUNT   (1 << 20)

#define QUERY_COUNT   (1 << 7)

#define THREAD_NUBMER 4

#define TEST_LOOKUP   1

#define RANDOM_KEYS   1

#if RANDOM_KEYS
uint64_t *loads;
#endif

long total_time;

uint64_t exists = 0;

uint64_t update = 0;

uint64_t failure = 0;

uint64_t total_count = TOTAL_COUNT;

int thread_number = THREAD_NUBMER;

TMHashMapByRef<uint64_t, uint64_t, pmdk::PMDKTM, pmdk::persist> *set;

stringstream *output;

atomic<int> stopMeasure(0);

struct target {
    int tid;
    TMHashMapByRef<uint64_t, uint64_t, pmdk::PMDKTM, pmdk::persist> *set;
};

void simpleInsert() {
    for (int i = 0; i < total_count; i++) {
#if RANDOM_KEYS
        set->add(loads[i]);
#else
        set->add(i);
#endif
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
#if RANDOM_KEYS
        if (!work->set->add(loads[i])) {
#else
            if (!work->set->add(i)) {
#endif
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
    while (stopMeasure.load(memory_order_relaxed) == 0) {
        for (int i = work->tid; i < total_count; i += thread_number) {
#if TEST_LOOKUP
#if RANDOM_KEYS
            if (work->set->contains(loads[i])) {
#else
                if (work->set->contains(i)) {
#endif
                hit++;
            } else {
                fail++;
            }
#else
#if RANDOM_KEYS
            if (work->set->remove(loads[i])) {
                hit++;
                if (!work->set->add(loads[i])) {
                    fail++;
                }
            }
#else
        if (work->set->remove(i)) {
            hit++;
            if (!work->set->add(i)) {
                fail++;
            }
        }
#endif
#endif
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
    cout << "Measuring ..." << endl;
    Timer timer;
    timer.start();
    for (int i = 0; i < thread_number; i++) {
        pthread_create(&workers[i], nullptr, measureWorker, &parms[i]);
    }
    while (timer.elapsedSeconds() < default_timer_range) {
        sleep(1);
    }
    stopMeasure.store(1, memory_order_relaxed);
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
#if RANDOM_KEYS
    loads = new uint64_t[total_count];
    UniformGen<uint64_t>::generate(loads, total_count);
#endif
    output = new stringstream[thread_number];
    //set = new TMHashMapByRef<uint64_t, uint64_t, pmdk::PMDKTM, pmdk::persist>(1000);
    pmdk::PMDKTM::updateTx([&]() {
        set = pmdk::PMDKTM::tmNew<TMHashMapByRef<uint64_t, uint64_t, pmdk::PMDKTM, pmdk::persist>>(1000);
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
    for (int i = 0; i < total_count; i++) {
        set->remove(i);
    }
    pmdk::PMDKTM::updateTx([&]() { pmdk::PMDKTM::tmDelete(set); });
    delete[] output;
#if RANDOM_KEYS
    delete[] loads;
#endif
}