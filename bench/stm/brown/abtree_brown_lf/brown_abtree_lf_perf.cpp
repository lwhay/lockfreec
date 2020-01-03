//
// Created by lwh on 19-3-14.
//

#include <iostream>
#include <limits>
#include <cassert>
#include <pthread.h>
#include <sys/time.h>

#include "adapter.h"
#include "perfparms.h"

#define MAX_ELEMENT_NUM (1 << TOTAL_ELEMENT_POWER)
const int NUM_THREAD = TOTAL_THREAD_NUMBER;

size_t total_count = MAX_ELEMENT_NUM;
int threads_num = NUM_THREAD;

typedef struct tree_struct {
    ds_adapter<int, void *> *tree;
    int tid;
} treeStruct;

void *insertWorker(void *args) {
    treeStruct *ts = (treeStruct *) args;
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < (total_count / NUM_THREAD); i++) {
        int key = i * NUM_THREAD + ts->tid;
        ts->tree->insertIfAbsent(ts->tid, key, (void *) key);
        if (i % (1 << NICE_PRINT_POWER) == 0) {
            struct timeval end;
            gettimeofday(&end, nullptr);
            std::cout << ts->tid << " " << i << " "
                      << (end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec << std::endl;
            gettimeofday(&begin, nullptr);
        }
    }
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
    return 0;
}