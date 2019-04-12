//
// Created by Michael on 2019/4/11.
//

#include <sys/time.h>
#include <atomic>
#include <iostream>
#include <sstream>
#include <random>

using namespace std;

stringstream *output;

#define TOTAL_LOAD (1 << 28)
#define THREAD_NUM 4
#define TEST_CAS_WEAK 1

atomic<uint64_t> total_opers;

atomic<uint64_t> global_points;

uint64_t local_points[THREAD_NUM];

uint64_t local_others[THREAD_NUM];

void initPayload() {
    global_points = (uint64_t) 0;
    for (int i = 0; i < THREAD_NUM; i++) {
        local_points[i] = (uint64_t) i;
        local_others[i] = (uint64_t) ((i + 1) % THREAD_NUM);
        cout << "i " << i << " " << local_points[i] << " " << local_others[i] << endl;
    }
}

void *dcasWorker(void *args) {
    int tid = *((int *) args);
    bool cas_result;
    uint64_t self_opers = 0;
    uint64_t self_tries = 0;
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < TOTAL_LOAD / THREAD_NUM; i++) {
        do {
#if TEST_CAS_WEAK
            cas_result = global_points.compare_exchange_weak(local_points[tid], local_others[tid]);
#else
            cas_result = global_points.compare_exchange_strong(local_points[tid], local_others[tid]);
#endif
            self_tries++;
            // For dcas, cas_result denotes whether an exchange operation has been sucessfully enforced by the current thread.
        } while (!cas_result);
        local_points[tid] = (local_points[tid] + 1) % THREAD_NUM;
        local_others[tid] = (local_others[tid] + 1) % THREAD_NUM;
        self_opers++;
        total_opers.fetch_add(1);
    }
    struct timeval end;
    gettimeofday(&end, nullptr);
    output[tid] << tid << ": " << self_opers << " " << self_tries << " "
                << ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec));
}

void testDCAS(int thread_num) {
    initPayload();
    output = new stringstream[thread_num];
    pthread_t threads[thread_num];
    int tids[thread_num];
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < thread_num; i++) {
        tids[i] = i;
        pthread_create(&threads[i], nullptr, dcasWorker, &tids[i]);
    }
    for (int i = 0; i < thread_num; i++) {
        pthread_join(threads[i], nullptr);
        string outstr = output[i].str();
        cout << outstr << endl;
    }
    struct timeval end;
    gettimeofday(&end, nullptr);
    cout << total_opers << " " << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << endl;
    delete[] output;
}

typedef uint32_t large_t;

struct uintlarge {
    large_t pointer;
    large_t counter;
};

void simpleTestStrong() {
    atomic<uint32_t> a;
    uint32_t v = 0;
    a = v;
    uint32_t sc = 0;
    uint32_t sv = 1;
    uint32_t n;
    cout << a << " " << v << " " << sc << " " << sv << " " << n << endl;
    a.exchange(sv);
    cout << a << " " << v << " " << sc << " " << sv << " " << n << endl;
    a.compare_exchange_weak(sv, sc);
    //a.load();
    //a.store(v);
    cout << a << " " << v << " " << sc << " " << sv << " " << n << endl;
    n = a;
    cout << a << " " << v << " " << sc << " " << sv << " " << n << endl;
    a.compare_exchange_strong(sc, sv);
    //ga.load();
    n = a;
    //ga.store(gv);
    cout << a << " " << v << " " << sc << " " << sv << " " << n << endl;
    // cas: comp, val
    atomic<uintlarge> ga;
    uintlarge gv;
    gv.pointer = 0;
    gv.counter = 0;
    ga = gv;
    uintlarge lc;
    lc.pointer = 0;
    lc.counter = 1;
    uintlarge lv;
    lv.pointer = 1;
    lv.counter = 0;
    uintlarge nv;
    cout << gv.pointer << " " << gv.counter << " " << lc.pointer << " " << lc.counter << " " << lv.pointer << " "
         << lv.counter << " " << nv.pointer << " " << nv.counter << endl;
    nv = ga.exchange(lv); // Success ga: 1 0
    cout << gv.pointer << " " << gv.counter << " " << lc.pointer << " " << lc.counter << " " << lv.pointer << " "
         << lv.counter << " " << nv.pointer << " " << nv.counter << endl;
    nv = ga;
    //ga.load();
    //ga.store(gv);
    cout << gv.pointer << " " << gv.counter << " " << lc.pointer << " " << lc.counter << " " << lv.pointer << " "
         << lv.counter << " " << nv.pointer << " " << nv.counter << endl;
    ga.compare_exchange_weak(lv, lc); // Success ga: 0 1
    gv = ga;
    //ga.load();
    //ga.store(gv);
    cout << gv.pointer << " " << gv.counter << " " << lc.pointer << " " << lc.counter << " " << lv.pointer << " "
         << lv.counter << " " << nv.pointer << " " << nv.counter << endl;
    ga.compare_exchange_strong(lv, lc); // Fail
    //ga.load();
    gv = ga;
    cout << gv.pointer << " " << gv.counter << " " << lc.pointer << " " << lc.counter << " " << lv.pointer << " "
         << lv.counter << " " << nv.pointer << " " << nv.counter << endl;
    //ga.store(gv);
    cout << gv.pointer << " " << gv.counter << " " << lc.pointer << " " << lc.counter << " " << lv.pointer << " "
         << lv.counter << " " << nv.pointer << " " << nv.counter << endl;
};

int main(int argc, char **argv) {
    simpleTestStrong();
    int thread_num = THREAD_NUM;
    if (argc > 1) {
        thread_num = atoi(argv[1]);
    }
    testDCAS(thread_num);
    return 0;
}