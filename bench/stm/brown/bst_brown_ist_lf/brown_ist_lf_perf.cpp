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
const int NUM_THREAD = 1;

typedef struct tree_struct {
    ds_adapter<int, void *> *tree;
    int tid;
} treeStruct;

void *insertWorker(void *args) {
    treeStruct *ts = (treeStruct *) args;
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < (MAX_ELEMENT_NUM / NUM_THREAD); i++) {
        int key = i * NUM_THREAD + ts->tid;
        ts->tree->insertIfAbsent(ts->tid, key, (void *) key);
        if (i % (1 << 15) == 0) {
            struct timeval end;
            gettimeofday(&end, nullptr);
            std::cout << ts->tid << " " << i << " "
                      << (end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec << std::endl;
            gettimeofday(&begin, nullptr);
        }
    }
}

int main(int argc, char **argv) {
    const int KEY_ANY = 0;
    const int unused1 = 0;
    void *unused2 = NULL;
    RandomFNV1A *const unused3 = NULL;
    auto tree = new ds_adapter<int, void *>(NUM_THREAD, KEY_ANY, unused1, unused2, unused3);

    pthread_t threads[NUM_THREAD];
    treeStruct tids[NUM_THREAD];
    for (int i = 0; i < NUM_THREAD; i++) {
        tids[i].tree = tree;
        tids[i].tid = i;
        tree->initThread(i);
        pthread_create(threads + i, NULL, insertWorker, tids + i);
    }
    for (int i = 0; i < NUM_THREAD; i++) {
        pthread_join(threads[i], NULL);
    }
    tree->printSummary();
    return 0;
}