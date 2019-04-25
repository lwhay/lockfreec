//
// Created by Michael on 2019-04-25.
//

#include <iostream>
#include <sstream>
#include <random>
#include "tracer.h"
#include "lf_basic.h"
#include "OneFileLF.h"

using namespace std;

using namespace oflf;

stringstream *output;

typedef unsigned long long int atom_t;

#define TOTAL_LOAD  (1 << 20)
#define THREAD_NUM  4
#define TEST_CAS    1
#define TEST_DCAS   0
#define TEST_OFDCAS 0
#define LQ_POINTER  0
#define LQ_COUNTER  1
#define LQ_PAC_SIZE 2

atom_t total_opers = 0;

atom_t global_points[LQ_PAC_SIZE];

atom_t local_points[THREAD_NUM][LQ_PAC_SIZE];

atom_t local_others[THREAD_NUM][LQ_PAC_SIZE];

void initPayload() {
    global_points[LQ_POINTER] = (atom_t) 0;
    global_points[LQ_COUNTER] = (atom_t) 0;
    for (int i = 0; i < THREAD_NUM; i++) {
        local_points[i][LQ_POINTER] = (atom_t) i;
        local_points[i][LQ_COUNTER] = (atom_t) i;
#if TEST_CAS
        local_others[i][LQ_POINTER] = (atom_t) (i + 1);
        local_others[i][LQ_POINTER] = (atom_t) (i + 1);
#elif TEST_DCAS
        local_others[i][LQ_POINTER] = (atom_t) ((i + 1) % THREAD_NUM);
        local_others[i][LQ_POINTER] = (atom_t) ((i + 1) % THREAD_NUM);
#elif TEST_OFDCAS
        local_others[i][LQ_POINTER] = (atom_t) ((i + 1) % THREAD_NUM);
        local_others[i][LQ_POINTER] = (atom_t) ((i + 1) % THREAD_NUM);
#endif
        cout << "i " << i << " " << local_points[i][LQ_POINTER] << " " << local_others[i][LQ_POINTER] << endl;
    }
}

void *dcasWorker(void *args) {
    int tid = *((int *) args);
#if TEST_CAS
    atom_t cas_result = 0;
#else
    unsigned char cas_result = 0;
#endif
    atom_t self_opers = 0;
    atom_t self_tries = 0;
    Tracer tracer;
    tracer.startTime();
    for (int i = 0; i < TOTAL_LOAD / THREAD_NUM; i++) {
        do {
#if TEST_CAS
            cas_result = __sync_val_compare_and_swap(&global_points[LQ_POINTER], local_points[tid][LQ_POINTER],
                                                     local_others[tid][LQ_POINTER]);
#elif TEST_DCAS
            cas_result = abstraction_dcas(global_points, local_points[tid], local_others[tid]);
#elif TEST_OFDCAS
            cas_result = DCAS(global_points, local_points[tid][LQ_POINTER], local_points[tid][LQ_COUNTER],
                              local_others[tid][LQ_POINTER], local_others[tid][LQ_COUNTER]);
#endif
            /*cout << tid << " " << global_points[LQ_POINTER] << " " << local_points[tid][LQ_POINTER] << " "
                 << local_others[tid][LQ_POINTER] << " " << (atom_t) cas_result << endl;*/
            self_tries++;
#if TEST_CAS
            // For __sync_val_compare_and_swap, we cannot guarantee cas_result to be changed by the current thread.
            // Thus, we need to guarantee any two threads result in different exchanged values.
        } while (cas_result != local_points[tid][LQ_POINTER]);
        local_points[tid][LQ_POINTER] += THREAD_NUM;
        local_points[tid][LQ_COUNTER] += THREAD_NUM;
        local_others[tid][LQ_POINTER] += THREAD_NUM;
        local_others[tid][LQ_COUNTER] += THREAD_NUM;
#elif TEST_DCAS
        // For dcas, cas_result denotes whether an exchange operation has been sucessfully enforced by the current thread.
    } while (cas_result == 0);
    local_points[tid][LQ_POINTER] = (local_points[tid][LQ_POINTER] + 1) % THREAD_NUM;
    local_points[tid][LQ_COUNTER] = (local_points[tid][LQ_COUNTER] + 1) % THREAD_NUM;
    local_others[tid][LQ_POINTER] = (local_others[tid][LQ_POINTER] + 1) % THREAD_NUM;
    local_others[tid][LQ_COUNTER] = (local_others[tid][LQ_COUNTER] + 1) % THREAD_NUM;
#elif TEST_OFDCAS
        // For dcas, cas_result denotes whether an exchange operation has been sucessfully enforced by the current thread.
    } while (cas_result == 0);
    local_points[tid][LQ_POINTER] = (local_points[tid][LQ_POINTER] + 1) % THREAD_NUM;
    local_points[tid][LQ_COUNTER] = (local_points[tid][LQ_COUNTER] + 1) % THREAD_NUM;
    local_others[tid][LQ_POINTER] = (local_others[tid][LQ_POINTER] + 1) % THREAD_NUM;
    local_others[tid][LQ_COUNTER] = (local_others[tid][LQ_COUNTER] + 1) % THREAD_NUM;
#endif
        self_opers++;
        __sync_fetch_and_add(&total_opers, 1);
    }
    output[tid] << tid << ": " << self_opers << " " << self_tries << " " << tracer.getRunTime();
}

void testDCAS() {
    initPayload();
    output = new stringstream[THREAD_NUM];
    pthread_t threads[THREAD_NUM];
    int tids[THREAD_NUM];
    Tracer tracer;
    tracer.startTime();
    for (int i = 0; i < THREAD_NUM; i++) {
        tids[i] = i;
        pthread_create(&threads[i], nullptr, dcasWorker, &tids[i]);
    }
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(threads[i], nullptr);
        string outstr = output[i].str();
        cout << outstr << endl;
    }
    cout << total_opers << " " << tracer.getRunTime() << endl;
    delete[] output;
}

int main(int argc, char **argv) {
    testDCAS();
    return 0;
}