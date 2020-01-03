//
// Created by lwh on 19-3-14.
//

#include <iostream>
#include <limits>
#include <cassert>
#include <pthread.h>
#include <sys/time.h>

#include "adapter.h"

#define MAX_ELEMENT_NUM (1 << 20)
#define MAX_THREADS_NUM (1 << 8)
const int NUM_THREAD = 4;

size_t total_count = MAX_ELEMENT_NUM;
int threads_num = NUM_THREAD;

typedef struct tree_struct {
    ds_adapter<int, void *> *tree;
    int tid;
} treeStruct;

long int thread_time[MAX_THREADS_NUM];

long int total_time = 0;

long int min_time = (1 << 30);

long int max_time = 0;

void *insertWorker(void *args) {
    treeStruct *ts = (treeStruct *) args;
    struct timeval begin;
    struct timeval tbegin;
    gettimeofday(&tbegin, nullptr);
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < (total_count / threads_num); i++) {
        int key = i * threads_num + ts->tid;
        ts->tree->insertIfAbsent(ts->tid, key, (void *) key);
        if (i % (1 << 15) == 0) {
            struct timeval end;
            gettimeofday(&end, nullptr);
            std::cout << ts->tid << " " << i << " "
                      << (end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec << std::endl;
            gettimeofday(&begin, nullptr);
        }
    }
    struct timeval tend;
    gettimeofday(&tend, nullptr);
    thread_time[ts->tid] = (tend.tv_sec - tbegin.tv_sec) * 1000000 + tend.tv_usec - tbegin.tv_usec;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        total_count = std::atol(argv[1]);
        threads_num = std::atoi(argv[2]);
    }
    std::cout << "total_count: " << total_count << " threads_num: " << threads_num << std::endl;
    const int KEY_RESERVED = std::numeric_limits<int>::min();
    const int unused1 = 0;
    void *const VALUE_RESERVED = NULL;
    RandomFNV1A *const unused2 = NULL;
    auto tree = new ds_adapter<int, void *>(threads_num, KEY_RESERVED, unused1, VALUE_RESERVED, unused2);

    pthread_t threads[threads_num];
    treeStruct tids[threads_num];
    for (int i = 0; i < threads_num; i++) {
        tids[i].tree = tree;
        tids[i].tid = i;
        tree->initThread(i);
        pthread_create(threads + i, NULL, insertWorker, tids + i);
    }
    for (int i = 0; i < threads_num; i++) {
        pthread_join(threads[i], NULL);
    }
    tree->printSummary();
    for (int i = 0; i < threads_num; i++) {
        if (max_time < thread_time[i]) {
            max_time = thread_time[i];
        }
        if (min_time > thread_time[i]) {
            min_time = thread_time[i];
        }
        total_time += thread_time[i];
    }
    std::cout << "Total: " << total_count << " Thread: " << threads_num << " AVG: " << (total_time / threads_num)
              << " MAX: " << max_time << " Min: " << min_time << std::endl;
    return 0;
}