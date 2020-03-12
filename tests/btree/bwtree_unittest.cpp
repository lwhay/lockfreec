//
// Created by iclab on 3/11/20.
//

#include "bwtree.h"
#include "tracer.h"

using namespace wangziqi2013;
using namespace bwtree;

class KeyComparator {
public:
    inline bool operator()(const long int k1, const long int k2) const {
        return k1 < k2;
    }

    KeyComparator(int dummy) {
        (void) dummy;

        return;
    }

    KeyComparator() = delete;
    //KeyComparator(const KeyComparator &p_key_cmp_obj) = delete;
};

class KeyEqualityChecker {
public:
    inline bool operator()(const long int k1, const long int k2) const {
        return k1 == k2;
    }

    KeyEqualityChecker(int dummy) {
        (void) dummy;

        return;
    }

    KeyEqualityChecker() = delete;
    //KeyEqualityChecker(const KeyEqualityChecker &p_key_eq_obj) = delete;
};

uint64_t total_count = (1llu << 27);
uint64_t thread_number = 1;

void SingleTest() {
    BwTree<uint64_t, uint64_t> *tree = new BwTree<uint64_t, uint64_t>();
    tree->UpdateThreadLocal(1);
    tree->AssignGCID(0);
    Tracer tracer;
    tracer.startTime();
    for (int i = 0; i < total_count; i++) tree->Insert(i, i);
    std::cout << total_count << " " << tracer.fetchTime() << " " << (double) total_count / tracer.fetchTime()
              << std::endl;
    delete tree;
}

void MultiTest() {
    BwTree<uint64_t, uint64_t> *tree = new BwTree<uint64_t, uint64_t>();
    tree->UpdateThreadLocal(thread_number);
    tree->AssignGCID(0);
    std::vector<std::thread> threads;
    for (size_t t = 0; t < thread_number; t++) {
        //tree->RegisterThread();
        threads.push_back(std::thread([](BwTree<uint64_t, uint64_t> *tree, size_t tid) {
            for (int i = tid; i < total_count; i += thread_number) tree->Insert(i, i);
        }, tree, t));
    }
    for (size_t t = 0; t < thread_number; t++) {
        threads[t].join();
    }
    delete tree;
}

int main(int argc, char **argv) {
    SingleTest();
//    MultiTest();
    return 0;
}
