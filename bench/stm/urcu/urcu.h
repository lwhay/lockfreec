//
// Created by lwh on 19-3-12.
//

#ifndef URCU_H
#define URCU_H

typedef struct rcu_node_t {
    //char p1[128];
    union {
        struct {
            volatile unsigned long time;
            volatile uint64_t val1;
            volatile uint64_t val2;
        };
        char bytes[192];
    };
    //char p2[128-sizeof(time)- 2*sizeof(uint64_t)];
} rcu_node;

namespace urcu {

    void init(const int numThreads);

    void deinit(const int numThreads);

    void readLock();

    void readUnlock();

    void synchronize();

    void registerThread(int id);

    void unregisterThread();

}

#endif /* URCU_H */
