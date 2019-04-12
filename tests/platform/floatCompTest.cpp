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

#define PSEUDO_NUM (1LLU << 20)
#define TOTAL_LOAD (1LLU << 32)
#define THREAD_NUM 4
#define TEST_CAS_WEAK 1

atomic<uint64_t> total_opers;

float pseudoNumber[PSEUDO_NUM];

float *locals;

void initPayload(int thread_num) {
    locals = new float[thread_num];
    for (int i = 0; i < PSEUDO_NUM; i++) {
        pseudoNumber[i] = (float) rand() / (1LLU << 31);
    }
}

void simpleTest() {
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    uint64_t counter = 0;
    float local = .0f;
    for (int i = 0; i < TOTAL_LOAD / PSEUDO_NUM; i++) {
        for (int j = 0; j < PSEUDO_NUM; j++) {
            if (local > (1LLU << 32)) {
                local = .0f;
            } else if (local < -(1 < 30)) {
                local = .0f;
            }
            switch (j % 4) {
                case 0:
                    local += pseudoNumber[j];
                    break;
                case 1:
                    local /= pseudoNumber[j];
                    break;
                case 2:
                    local -= pseudoNumber[j];
                    break;
                case 3:
                    local *= pseudoNumber[j];
                    break;
            }
            counter++;
        }
    }
    struct timeval end;
    gettimeofday(&end, nullptr);
    cout << local << " " << counter << " " << ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec))
         << endl;
}

void *dcasWorker(void *args) {
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    int tid = *((int *) args);
    locals[tid] = .0f;
    unsigned long long counter = 0;
    for (int i = 0; i < TOTAL_LOAD / PSEUDO_NUM; i++) {
        for (int j = 0; j < PSEUDO_NUM; j++) {
            if (locals[tid] > (1LLU << 32)) {
                locals[tid] = .0f;
            } else if (locals[tid] < -(1 < 30)) {
                locals[tid] = .0f;
            }
            switch (j % 4) {
                case 0:
                    locals[tid] += pseudoNumber[j];
                    break;
                case 1:
                    locals[tid] /= pseudoNumber[j];
                    break;
                case 2:
                    locals[tid] -= pseudoNumber[j];
                    break;
                case 3:
                    locals[tid] *= pseudoNumber[j];
                    break;
            }
            counter++;
        }
    }
    total_opers.fetch_add(counter, std::memory_order_seq_cst);
    struct timeval end;
    gettimeofday(&end, nullptr);
    output[tid] << "\t" << tid << " " << ((end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec));
}

void testfloat(int thread_num) {
    output = new stringstream[thread_num];
    pthread_t threads[thread_num];
    int tids[thread_num];
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < thread_num; i++) {
        tids[i] = i;
        pthread_create(&threads[i], nullptr, dcasWorker, &tids[i]);
    }
    float global = .0f;
    for (int i = 0; i < thread_num; i++) {
        pthread_join(threads[i], nullptr);
        global += locals[i];
        string outstr = output[i].str();
        cout << outstr << endl;
    }
    struct timeval end;
    gettimeofday(&end, nullptr);
    cout << total_opers << " " << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << endl;
    delete[] output;
}

int main(int argc, char **argv) {
    int thread_num = THREAD_NUM;
    if (argc > 1) {
        thread_num = atoi(argv[1]);
    }
    initPayload(thread_num);
    simpleTest();
    testfloat(thread_num);
    delete locals;
    return 0;
}