//
// Created by Michael on 2019-04-25.
//

#include <iostream>
#include "LFResizableHashSet.h"

#define TOTAL_COUNT   (1 << 20)

#define QUERY_COUNT   (1 << 10)

#define THREAD_NUBMER 4

#define SINGLE_INSERT 0

struct target {
    int tid;
    OFLFResizableHashSet<uint64_t> *set;
};

void simpleInsert() {
    OFLFResizableHashSet<uint64_t> set;
    for (int i = 0; i < TOTAL_COUNT; i++) {
        set.add(i);
    }
    for (int i = 0; i < TOTAL_COUNT; i += (TOTAL_COUNT / QUERY_COUNT)) {
        cout << set.contains(i);
        if (i / (TOTAL_COUNT / QUERY_COUNT) % 32 == 0) {
            cout << endl;
        }
    }
}

void *insertWorker(void *args) {
    struct target *work = (struct target *) args;
    for (int i = work->tid; i < TOTAL_COUNT; i += THREAD_NUBMER) {
        work->set->add(i);
    }
}

void multiInsert() {
    pthread_t workers[THREAD_NUBMER];
    OFLFResizableHashSet<uint64_t> set;
    struct target parms[THREAD_NUBMER];
    for (int i = 0; i < THREAD_NUBMER; i++) {
        parms[i].tid = i;
        parms[i].set = &set;
        pthread_create(&workers[i], nullptr, insertWorker, &parms[i]);
    }
    for (int i = 0; i < THREAD_NUBMER; i++) {
        pthread_join(workers[i], nullptr);
    }
}

int main(int argc, char **argv) {
#if SINGLE_INSERT
    simpleInsert();
#else
    multiInsert();
#endif
}