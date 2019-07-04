#include <iostream>
#include <sstream>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_set>
#include "tracer.h"
#include "basic_hash_table.h"
#include "atomic_shared_ptr.h"

#define DEFAULT_HASH_LEVEL (25)
#define DEFAULT_THREAD_NUM (8)
#define DEFAULT_KEYS_COUNT (1 << 20)
#define DEFAULT_KEYS_RANGE (1 << 2)

#define DEFAULT_STR_LENGTH 256
//#define DEFAULT_KEY_LENGTH 8

#define TEST_LOOKUP        0

#define COUNT_HASH         2

#define DEFAULT_STORE_BASE 100000000

#if COUNT_HASH == 1
#define UNSAFE
#ifdef UNSAFE
neatlib::ConcurrentHashTable<uint64_t, uint64_t,
        std::hash<uint64_t>, 4, 16,
        neatlib::unsafe::atomic_shared_ptr,
        neatlib::unsafe::shared_ptr> *mhash;
#else
neatlib::ConcurrentHashTable<uint64_t, uint64_t, std::hash<size_t>, 4, 16> *mhash;
#endif
#elif COUNT_HASH == 2
neatlib::BasicHashTable<uint64_t,
        uint64_t,
        std::hash<uint64_t>,
        std::equal_to<uint64_t>,
        std::allocator<std::pair<const uint64_t, uint64_t>>, 4> *mhash;
#else
neatlib::LockFreeHashTable<uint64_t, uint64_t, std::hash<uint64_t>, 4, 16> *mhash;
#endif

uint64_t *loads;

long total_time;

uint64_t exists = 0;

uint64_t success = 0;

uint64_t failure = 0;

uint64_t total_count = DEFAULT_KEYS_COUNT;

int thread_number = DEFAULT_THREAD_NUM;

int key_range = DEFAULT_KEYS_RANGE;

stringstream *output;

atomic<int> stopMeasure(0);

struct target {
    int tid;
    uint64_t *insert;
#if COUNT_HASH == 1
#ifdef UNSAFE
    neatlib::ConcurrentHashTable<uint64_t, uint64_t,
            std::hash<uint64_t>, 4, 16,
            neatlib::unsafe::atomic_shared_ptr,
            neatlib::unsafe::shared_ptr> *store;
#else
    neatlib::ConcurrentHashTable<uint64_t, uint64_t, std::hash<size_t>, 4, 16> *store;
#endif
#elif COUNT_HASH == 2
    neatlib::BasicHashTable<uint64_t,
            uint64_t,
            std::hash<uint64_t>,
            std::equal_to<uint64_t>,
            std::allocator<std::pair<const uint64_t, uint64_t>>, 4> *store;
#else
    neatlib::LockFreeHashTable<uint64_t, uint64_t, std::hash<uint64_t>, 4, 16> *store;
#endif
};

pthread_t *workers;

struct target *parms;

void simpleInsert() {
    Tracer tracer;
    tracer.startTime();
    int inserted = 0;
    unordered_set<uint64_t> set;
    for (int i = 0; i < total_count; i++) {
        mhash->Insert(loads[i], loads[i]);
        set.insert(loads[i]);
        inserted++;
    }
    cout << inserted << " " << tracer.getRunTime() << endl;
}

void *insertWorker(void *args) {
    //struct target *work = (struct target *) args;
    uint64_t inserted = 0;
    for (int i = 0; i < total_count; i++) {
        mhash->Insert(loads[i], loads[i]);
        inserted++;
    }
    __sync_fetch_and_add(&exists, inserted);
}

void *measureWorker(void *args) {
    Tracer tracer;
    tracer.startTime();
    struct target *work = (struct target *) args;
    uint64_t hit = 0;
    uint64_t fail = 0;
    try {
        while (stopMeasure.load(memory_order_relaxed) == 0) {
            for (int i = 0; i < total_count; i++) {
#if TEST_LOOKUP
                std::pair<uint64_t, uint64_t> ret = mhash->Get(loads[i]);
                /*if (ret.second != loads[i]) {
                    sleep(work->tid * 10);
                    cout << "\t" << loads[i] << " " << ret.first << " " << ret.second << endl;
                    //exit(0);
                }*/
                if (ret.second == loads[i])
                    hit++;
                else
                    fail++;
#else
                if (mhash->Update(loads[i], loads[i]))
                    hit++;
                else
                    fail++;
#endif
            }
        }
    } catch (exception e) {
        cout << work->tid << endl;
    }

    long elipsed = tracer.getRunTime();
    output[work->tid] << work->tid << " " << elipsed << " " << hit << endl;
    __sync_fetch_and_add(&total_time, elipsed);
    __sync_fetch_and_add(&success, hit);
    __sync_fetch_and_add(&failure, fail);
}

void prepare() {
    cout << "prepare" << endl;
    workers = new pthread_t[thread_number];
    parms = new struct target[thread_number];
    output = new stringstream[thread_number];
    for (int i = 0; i < thread_number; i++) {
        parms[i].tid = i;
        parms[i].store = mhash;
        parms[i].insert = (uint64_t *) calloc(total_count / thread_number, sizeof(uint64_t *));
        char buf[DEFAULT_STR_LENGTH];
        for (int j = 0; j < total_count / thread_number; j++) {
            std::sprintf(buf, "%d", i + j * thread_number);
            parms[i].insert[j] = j;
        }
    }
}

void finish() {
    cout << "finish" << endl;
    for (int i = 0; i < thread_number; i++) {
        delete[] parms[i].insert;
    }
    delete[] parms;
    delete[] workers;
    delete[] output;
}

void multiWorkers() {
    output = new stringstream[thread_number];
    Tracer tracer;
    tracer.startTime();
    for (int i = 0; i < thread_number; i++) {
        pthread_create(&workers[i], nullptr, insertWorker, &parms[i]);
    }
    for (int i = 0; i < thread_number; i++) {
        pthread_join(workers[i], nullptr);
    }
    cout << "Insert " << exists << " " << tracer.getRunTime() << endl;
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
    if (argc > 3) {
        thread_number = std::atol(argv[1]);
        key_range = std::atol(argv[2]);
        total_count = std::atol(argv[3]);
    }
#if COUNT_HASH == 1
#ifdef UNSAFE
    mhash = new neatlib::ConcurrentHashTable<uint64_t, uint64_t,
            std::hash<uint64_t>, 4, 16,
            neatlib::unsafe::atomic_shared_ptr,
            neatlib::unsafe::shared_ptr>();
#else
    mhash = new neatlib::ConcurrentHashTable<uint64_t, uint64_t, std::hash<uint64_t>, 4, 16>();
#endif
#elif COUNT_HASH == 2
    mhash = new neatlib::BasicHashTable<uint64_t,
            uint64_t,
            std::hash<uint64_t>,
            std::equal_to<uint64_t>,
            std::allocator<std::pair<const uint64_t, uint64_t>>, 4>();
#else
    mhash = new neatlib::LockFreeHashTable<uint64_t, uint64_t, std::hash<uint64_t>, 4, 16>(thread_number);
#endif
    cout << " threads: " << thread_number << " range: " << key_range << " count: " << total_count << endl;
    loads = (uint64_t *) calloc(total_count, sizeof(uint64_t));
    UniformGen<uint64_t>::generate(loads, key_range, total_count);
    prepare();
    cout << "simple" << endl;
    simpleInsert();
    cout << "multiinsert" << endl;
    multiWorkers();
    cout << "operations: " << success << " failure: " << failure << " throughput: "
         << (double) (success + failure) * thread_number / total_time << endl;
    free(loads);
    finish();
    //delete mhash;
    return 0;
}