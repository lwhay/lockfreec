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

uint64_t total_count = (1llu << 20);
uint64_t thread_number = 1;
uint64_t duplicate = (1 << 3);

void SingleUniqueTest() {
    BwTree<uint64_t, uint64_t> *tree = new BwTree<uint64_t, uint64_t>();
    tree->UpdateThreadLocal(1);
    tree->AssignGCID(0);
    Tracer tracer;
    tracer.startTime();
    for (int i = 0; i < total_count; i++) {
        tree->Insert(i, i);
        Tracer it;
        if (i % 10000000 == 0) {
            it.startTime();
            tree->PerformGC(0);
            std::cout << "\t" << it.getRunTime() << std::endl;
        }
    }
    std::cout << total_count << " " << tracer.fetchTime() << " " << ((double) total_count / tracer.fetchTime())
              << std::endl;

    tracer.startTime();
    uint64_t correct = 0;
    for (int i = 0; i < total_count; i++) {
        std::vector<uint64_t> ret;
        tree->GetValue(i, ret);
        if (ret[0] == i) correct++;
    }
    std::cout << total_count << " " << correct << " " << tracer.fetchTime() << " "
              << ((double) total_count / tracer.fetchTime()) << std::endl;

    tracer.startTime();
    correct = 0;
    for (int i = total_count - 1; i >= 0; i--) {
        std::vector<uint64_t> ret;
        tree->GetValue(i, ret);
        if (ret[0] == i) correct++;
    }
    std::cout << total_count << " " << correct << " " << tracer.fetchTime() << " "
              << ((double) total_count / tracer.fetchTime()) << std::endl;
    delete tree;
}

void SingleDuplicateTest() {
    BwTree<uint64_t, uint64_t> *tree = new BwTree<uint64_t, uint64_t>();
    tree->UpdateThreadLocal(1);
    tree->AssignGCID(0);
    Tracer tracer;
    tracer.startTime();
    for (int i = 0; i < total_count / duplicate; i++) {
        for (int j = 0; j < duplicate; j++) tree->Insert(i, i + j);
        Tracer it;
        if (i % 10000000 == 0) {
            it.startTime();
            tree->PerformGC(0);
            std::cout << "\t" << it.getRunTime() << std::endl;
        }
    }
    std::cout << total_count << " " << tracer.fetchTime() << " " << ((double) total_count / tracer.fetchTime())
              << std::endl;

    tracer.startTime();
    uint64_t correct = 0;
    for (int i = 0; i < total_count / duplicate; i++) {
        std::vector<uint64_t> ret;
        tree->GetValue(i, ret);
        for (int j = 0; j < duplicate; j++) if (ret[j] == (duplicate + i - j - 1)) correct++;
        if (i == 1000) for (int j = 0; j < duplicate; j++) std::cout << "\t" << ret[j] << std::endl;
    }
    std::cout << total_count << " " << correct << " " << tracer.fetchTime() << " "
              << ((double) total_count / tracer.fetchTime()) << std::endl;

    tracer.startTime();
    correct = 0;
    for (int i = total_count / duplicate - 1; i >= 0; i--) {
        std::vector<uint64_t> ret;
        tree->GetValue(i, ret);
        for (int j = 0; j < duplicate; j++) if (ret[j] == (duplicate + i - j - 1)) correct++;
    }
    std::cout << total_count << " " << correct << " " << tracer.fetchTime() << " "
              << ((double) total_count / tracer.fetchTime()) << std::endl;
    delete tree;
}

atomic<long> total_time{0};
atomic<long> total_tick{0};

void MultiTest() {
    using treetype = BwTree<uint64_t, uint64_t, KeyComparator, KeyEqualityChecker>;
    treetype *tree = new BwTree<uint64_t, uint64_t, KeyComparator, KeyEqualityChecker>(
            true, KeyComparator{1}, KeyEqualityChecker{1});
    tree->UpdateThreadLocal(thread_number);
    std::vector<std::thread> threads;
    for (size_t t = 0; t < thread_number; t++) {
        //tree->RegisterThread();
        threads.push_back(std::thread([](treetype *tree, size_t tid) {
            Tracer tracer;
            tracer.startTime();
            tree->AssignGCID(tid);
            long tick = 0;
            for (uint64_t i = tid; i < total_count; i += thread_number) {
                tree->Insert(i, i);
                tick++;
            }
            total_time.fetch_add(tracer.getRunTime());
            total_tick.fetch_add(tick);
        }, tree, t));
    }
    for (size_t t = 0; t < thread_number; t++) {
        threads[t].join();
        tree->UnregisterThread(t);
    }
    std::cout << "Insert: " << total_tick << " " << (total_time / thread_number) << " "
              << (double) total_tick.load() * thread_number / total_time.load() << std::endl;

    total_tick.store(0);
    total_time.store(0);
    threads.clear();
    for (size_t t = 0; t < thread_number; t++) {
        //tree->RegisterThread();
        threads.push_back(std::thread([](treetype *tree, size_t tid) {
            Tracer tracer;
            tracer.startTime();
            tree->AssignGCID(tid);
            long tick = 0;
            for (uint64_t i = tid; i < total_count; i += thread_number) {
                std::vector<uint64_t> ret;
                tree->GetValue(i, ret);
                assert(ret[0] == i);
                tick++;
            }
            total_time.fetch_add(tracer.getRunTime());
            total_tick.fetch_add(tick);
        }, tree, t));
    }
    for (size_t t = 0; t < thread_number; t++) {
        threads[t].join();
        tree->UnregisterThread(t);
    }
    std::cout << "Read: " << total_tick << " " << (total_time / thread_number) << " "
              << (double) total_tick.load() * thread_number / total_time.load() << std::endl;
    delete tree;
}

int main(int argc, char **argv) {
    SingleUniqueTest();
    SingleDuplicateTest();
    if (argc > 2) {
        thread_number = std::atol(argv[1]);
        total_count = std::atol(argv[2]);
    }
    MultiTest();
    return 0;
}
