//
// Created by Michael on 2019-04-25.
//

#include <iostream>
#include <sstream>
#include <random>
#include "tracer.h"
#include "lf_basic.h"

using namespace std;

stringstream *output;

stringstream *failure;

stringstream *trdinf;

typedef unsigned long long int atom_t;

#define PRINT_INFO  0
#define PRINT_FAIL  0

/*#define TEST_ADD    1
#define TEST_CAS    0
#define TEST_DCAS   0
#define TEST_OFDCAS 0*/

#define TEST_TYPE   1 // 0: TEST_ADD, 1: TEST_CAS, 2: TEST_DCAS, 3: TEST_OFDCAS, 4: TEST_ACAS; 5: TO BE CONT.

#define SINGLETEST  0

#if TEST_TYPE == 3

#include "OneFileLF.h"

using namespace oflf;
#endif

#define MAX_THRNUM  (1 << 8)
#define TOTAL_LOAD  (1 << 20)
#define PRINT_TICK  (1 << 4)
#define THREAD_NUM  2
#define LQ_POINTER  0
#define LQ_COUNTER  1
#define LQ_PAC_SIZE 2

#define ALIGN_SIZE  16

uint64_t total_operations = TOTAL_LOAD;

uint64_t total_threads = THREAD_NUM;

atom_t total_opers = 0;

atom_t total_tries = 0;

atom_t global_points[LQ_PAC_SIZE];

atom_t local_target[MAX_THRNUM * ALIGN_SIZE][LQ_PAC_SIZE];

atom_t local_points[MAX_THRNUM * ALIGN_SIZE][LQ_PAC_SIZE];

atom_t local_others[MAX_THRNUM * ALIGN_SIZE][LQ_PAC_SIZE];

void initPayload() {
    global_points[LQ_POINTER] = (atom_t) 0;
    global_points[LQ_COUNTER] = (atom_t) 0;
    for (int i = 0; i < total_threads; i++) {
        local_target[i * ALIGN_SIZE][LQ_POINTER] = (atom_t) i;
        local_target[i * ALIGN_SIZE][LQ_COUNTER] = (atom_t) i;
        local_points[i * ALIGN_SIZE][LQ_POINTER] = (atom_t) i;
        local_points[i * ALIGN_SIZE][LQ_COUNTER] = (atom_t) i;
#if TEST_TYPE == 1
        local_others[i * ALIGN_SIZE][LQ_POINTER] = (atom_t) (i + 1);
        local_others[i * ALIGN_SIZE][LQ_POINTER] = (atom_t) (i + 1);
#elif TEST_TYPE == 2
        local_others[i * ALIGN_SIZE][LQ_POINTER] = (atom_t) ((i + 1) % total_threads);
        local_others[i * ALIGN_SIZE][LQ_POINTER] = (atom_t) ((i + 1) % total_threads);
#elif TEST_TYPE == 3
        local_others[i * ALIGN_SIZE][LQ_POINTER] = (atom_t) ((i + 1) % total_threads);
        local_others[i * ALIGN_SIZE][LQ_COUNTER] = (atom_t) ((i + 1) % total_threads);
#endif
        cout << "i " << i << " " << local_points[i * ALIGN_SIZE][LQ_POINTER] << " "
             << local_others[i * ALIGN_SIZE][LQ_POINTER] << endl;
    }
}

void *dcasWorker(void *args) {
    int tid = *((int *) args);
#if TEST_TYPE == 1
    atom_t cas_result = 0;
#else
    unsigned char cas_result = 0;
#endif
    atom_t self_opers = 0;
    atom_t self_tries = 0;
    Tracer tracer;
    tracer.startTime();
    for (int i = 0; i < total_operations / total_threads; i++) {
        atom_t *target;
        do {
#if SINGLETEST == 1
            target = global_points;
#else
            target = local_target[tid * ALIGN_SIZE];
#endif
#if TEST_TYPE == 0
            __sync_fetch_and_add(target, 1);
#elif TEST_TYPE == 1
            local_others[tid * ALIGN_SIZE][LQ_POINTER] = target[LQ_POINTER];
            cas_result = __sync_val_compare_and_swap(&target[LQ_POINTER],
                                                     local_others[tid * ALIGN_SIZE][LQ_POINTER],
                                                     local_points[tid * ALIGN_SIZE][LQ_POINTER]);
#elif TEST_TYPE == 2
            local_others[tid * ALIGN_SIZE][LQ_POINTER] = target[LQ_POINTER];
            local_others[tid * ALIGN_SIZE][LQ_COUNTER] = target[LQ_COUNTER];
            cas_result = abstraction_dcas(target, local_points[tid * ALIGN_SIZE],
                                          local_others[tid * ALIGN_SIZE]);
#elif TEST_TYPE == 3
            local_others[tid * ALIGN_SIZE][LQ_POINTER] = target[LQ_POINTER];
            local_others[tid * ALIGN_SIZE][LQ_COUNTER] = target[LQ_COUNTER];
            cas_result = DCAS(target, global_points[LQ_POINTER], global_points[LQ_COUNTER],
                              local_others[tid * ALIGN_SIZE][LQ_POINTER], local_others[tid * ALIGN_SIZE][LQ_COUNTER]);
#endif
#if PRINT_FAIL
            if (TEST_TYPE == 1 && (cas_result != local_points[tid* ALIGN_SIZE][LQ_POINTER]) || cas_result == 0) {
                failure[tid] << tid << " " << self_tries << " " << global_points[LQ_POINTER] << " "
                            << global_points[LQ_COUNTER] << " " << local_points[tid* ALIGN_SIZE][LQ_POINTER] << " "
                            << local_points[tid* ALIGN_SIZE][LQ_COUNTER] << " " << local_others[tid* ALIGN_SIZE][LQ_POINTER] << " "
                            << local_others[tid* ALIGN_SIZE][LQ_COUNTER] << " " << (atom_t) cas_result << endl;
            }
#endif
            self_tries++;
#if TEST_TYPE == 0
            } while (false);
#elif TEST_TYPE == 1
            // For __sync_val_compare_and_swap, we cannot guarantee cas_result to be changed by the current thread.
            // Thus, we need to guarantee any two threads result in different exchanged values.
        } while (cas_result != local_points[tid * ALIGN_SIZE][LQ_POINTER]);
        local_points[tid * ALIGN_SIZE][LQ_POINTER] += total_threads;
        local_points[tid * ALIGN_SIZE][LQ_COUNTER] += total_threads;
        local_others[tid * ALIGN_SIZE][LQ_POINTER] += total_threads;
        local_others[tid * ALIGN_SIZE][LQ_COUNTER] += total_threads;
#elif TEST_TYPE == 2
            // For dcas, cas_result denotes whether an exchange operation has been sucessfully enforced by the current thread.
        } while (cas_result == 0);
#if PRINT_INFO
        // Need to be augured, whether the atomic cas is really atomic when comparing two GLOBAL numbers.
        if (!__sync_bool_compare_and_swap(&global_points[LQ_POINTER], global_points[LQ_COUNTER],
                                          global_points[LQ_POINTER])) {
            output[tid] << "\033[1;31m" << "\t" << tid << ":" << global_points[LQ_POINTER] << ":"
                        << global_points[LQ_COUNTER] << "\033[0m" << endl;
        }
#endif
        local_points[tid *
                     ALIGN_SIZE][LQ_POINTER] += total_threads;//= (local_points[tid][LQ_POINTER] + 1) % total_threads;
        local_points[tid *
                     ALIGN_SIZE][LQ_COUNTER] += total_threads;//= (local_points[tid][LQ_COUNTER] + 1) % total_threads;
        local_others[tid *
                     ALIGN_SIZE][LQ_POINTER] += total_threads;//= (local_others[tid][LQ_POINTER] + 1) % total_threads;
        local_others[tid *
                     ALIGN_SIZE][LQ_COUNTER] += total_threads;//= (local_others[tid][LQ_COUNTER] + 1) % total_threads;
#elif TEST_TYPE == 3
        // For dcas, cas_result denotes whether an exchange operation has been sucessfully enforced by the current thread.
    } while (cas_result == 0);
    local_points[tid *
                 ALIGN_SIZE][LQ_POINTER] += total_threads;//= (local_points[tid][LQ_POINTER] + 1) % total_threads;
    local_points[tid *
                 ALIGN_SIZE][LQ_COUNTER] += total_threads;//= (local_points[tid][LQ_COUNTER] + 1) % total_threads;
    local_others[tid *
                 ALIGN_SIZE][LQ_POINTER] += total_threads;//= (local_others[tid][LQ_POINTER] + 1) % total_threads;
    local_others[tid *
                 ALIGN_SIZE][LQ_COUNTER] += total_threads;//= (local_others[tid][LQ_COUNTER] + 1) % total_threads;
#endif
        self_opers++;
    }
    __sync_fetch_and_add(&total_opers, self_opers);
    __sync_fetch_and_add(&total_tries, self_tries);
    trdinf[tid] << tid << ": " << self_opers << " " << self_tries << " " << tracer.getRunTime() << endl;
}

void testDCAS() {
    initPayload();
    output = new stringstream[total_threads];
    failure = new stringstream[total_threads];
    trdinf = new stringstream[total_threads];
    pthread_t threads[total_threads];
    int tids[total_threads];
    Tracer tracer;
    tracer.startTime();
    for (int i = 0; i < total_threads; i++) {
        tids[i] = i;
        pthread_create(&threads[i], nullptr, dcasWorker, &tids[i]);
    }
    for (int i = 0; i < total_threads; i++) {
        pthread_join(threads[i], nullptr);
        string outstr = output[i].str();
        cout << outstr;
    }
    for (int i = 0; i < total_threads; i++) {
        string outstr = failure[i].str();
        cout << outstr;
    }
    for (int i = 0; i < total_threads; i++) {
        string outstr = trdinf[i].str();
        cout << outstr;
    }
    cout << "opers: " << total_opers << " tries: " << total_tries << " time: " << tracer.getRunTime() << " global: "
         << global_points[LQ_POINTER] << "<->" << global_points[LQ_COUNTER] << endl;
    delete[] output;
    delete[] failure;
    delete[] trdinf;
}

int main(int argc, char **argv) {
    if (argc >= 2) {
        total_threads = atoi(argv[1]);
        total_operations = atol(argv[2]);
    }
    testDCAS();
    return 0;
}