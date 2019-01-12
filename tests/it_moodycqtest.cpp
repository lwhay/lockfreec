//
// Created by Michael on 2019/1/12.
//
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <sys/time.h>
#include "moody/concurrentqueue.h"

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

struct MallocTrackingTraits : public ConcurrentQueueDefaultTraits {
    static inline void *malloc(std::size_t size) { return tracking_allocator::malloc(size); }

    static inline void free(void *ptr) { tracking_allocator::free(ptr); }
};

#define MAX_PUSH_COUNT (1 << 28)
#define MAX_THREAD_NUM 40

void simpleTest() {
    ConcurrentQueue<int, MallocTrackingTraits> q;
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < MAX_PUSH_COUNT; i++)
        q.enqueue(i);
    struct timeval end;
    gettimeofday(&end, nullptr);
    std::cout << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << std::endl;
}

ConcurrentQueue<int, MallocTrackingTraits> *cq;

std::stringstream *output;

void *simpleWorker(void *args) {
    int tid = ((int *) args)[0];
    int nid = ((int *) args)[1];
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    unsigned long counter = 0;
    for (int i = 0; i < MAX_PUSH_COUNT / nid; i++) {
        cq[nid].enqueue(i);
        counter++;
    }
    struct timeval end;
    gettimeofday(&end, nullptr);
    output[tid] << tid << " " << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << " "
                << counter;
}

void concurrentTest(int tnum) {
    output = new std::stringstream[tnum];
    int **tids = new int *[tnum];
    pthread_t *threads = new pthread_t[tnum];
    struct timeval begin;
    gettimeofday(&begin, nullptr);

    for (int i = 0; i < tnum; i++) {
        tids[i] = new int[2];
        tids[i][0] = i;
        tids[i][1] = tnum;
        pthread_create(&threads[i], nullptr, simpleWorker, tids[i]);
    }

    for (int i = 0; i < tnum; i++) {
        pthread_join(threads[i], nullptr);
        std::string outstr = output[i].str();
        std::cout << outstr << std::endl;
        delete[] tids[i];
    }

    struct timeval end;
    gettimeofday(&end, nullptr);
    std::cout << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << std::endl;

    delete[] output;
}

int main(int argc, char **argv) {
    cq = new ConcurrentQueue<int, MallocTrackingTraits>[MAX_THREAD_NUM];
    for (int t = 1; t <= MAX_THREAD_NUM; t++) {
        concurrentTest(t);
        int v;
        int counter = 0;
        struct timeval begin;
        gettimeofday(&begin, nullptr);
        unsigned long long prevsize = cq[t].size_approx();
        while (cq[t].try_dequeue(v)) counter++;
        struct timeval end;
        gettimeofday(&end, nullptr);
        std::cout << t << " " << prevsize << " " << counter << " " << cq[t].size_approx() << " "
                  << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << std::endl;
    }
    delete cq;
    return 0;
}