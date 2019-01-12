//
// Created by Michael on 2019/1/12.
//

#include <iostream>
#include <sstream>
#include <pthread.h>
#include <sys/time.h>
#include "moody/basiclockfreequeue.h"

struct tracking_allocator {
    union tag {
        std::size_t size;
        max_align_t dummy;        // GCC forgot to add it to std:: for a while
    };

    static inline void *malloc(std::size_t size) {
        auto ptr = std::malloc(size + sizeof(tag));
        reinterpret_cast<tag *>(ptr)->size = size;
        usage.fetch_add(size, std::memory_order_relaxed);
        return reinterpret_cast<char *>(ptr) + sizeof(tag);
    }

    static inline void free(void *ptr) {
        ptr = reinterpret_cast<char *>(ptr) - sizeof(tag);
        auto size = reinterpret_cast<tag *>(ptr)->size;
        usage.fetch_add(-size, std::memory_order_relaxed);
        std::free(ptr);
    }

    static inline std::size_t current_usage() { return usage.load(std::memory_order_relaxed); }

private:
    static std::atomic<std::size_t> usage;
};

std::atomic<std::size_t> tracking_allocator::usage(0);

#define MAX_PUSH_COUNT (1 << 28)
#define MAX_THREAD_NUM 40
#define USE_MOODY_TOKEN 1

void simpleTest() {
    BasicLockFreeQueue<int> q;
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < MAX_PUSH_COUNT; i++)
        q.enqueue(i);
    struct timeval end;
    gettimeofday(&end, nullptr);
    std::cout << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << std::endl;
}

std::stringstream *output;

struct param_struct {
    int tid;
    int nid;
    BasicLockFreeQueue<int> *q;
};

void *simpleWorker(void *args) {
    struct param_struct *psp = (struct param_struct *) args;
    int tid = psp->tid;
    int nid = psp->nid;
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    unsigned long counter = 0;
    for (int i = 0; i < MAX_PUSH_COUNT / nid; i++) {
        psp->q->enqueue(i);
        counter++;
    }
    struct timeval end;
    gettimeofday(&end, nullptr);
    output[tid] << tid << " " << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << " "
                << counter;
}

void concurrentTest(struct param_struct &ps) {
    output = new std::stringstream[ps.nid];
    struct param_struct *pds = new struct param_struct[ps.nid];
    pthread_t *threads = new pthread_t[ps.nid];
    struct timeval begin;
    for (int i = 0; i < ps.nid; i++) {
        pds[i].nid = ps.nid;
        pds[i].tid = i;
        pds[i].q = ps.q;
    }
    gettimeofday(&begin, nullptr);

    for (int i = 0; i < ps.nid; i++) {
        pthread_create(&threads[i], nullptr, simpleWorker, &pds[i]);
    }

    for (int i = 0; i < ps.nid; i++) {
        pthread_join(threads[i], nullptr);
        std::string outstr = output[i].str();
        std::cout << outstr << std::endl;
    }

    struct timeval end;
    gettimeofday(&end, nullptr);
    std::cout << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << std::endl;

    delete[] output;
}

int main(int argc, char **argv) {
    for (int t = 1; t <= MAX_THREAD_NUM; t++) {
        BasicLockFreeQueue<int> q;
        struct param_struct ps;
        ps.q = &q;
        ps.nid = t;
        concurrentTest(ps);
        int v;
        int counter = 0;
        struct timeval begin;
        gettimeofday(&begin, nullptr);
        while (ps.q->try_dequeue(v)) counter++;
        struct timeval end;
        gettimeofday(&end, nullptr);
        std::cout << t << " " << counter << " " << " "
                  << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << std::endl;
    }
    return 0;
}
