//
// Created by Michael on 2018/11/30.
//
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <boost/lockfree/queue.hpp>

#define MAX_PUSH_COUNT (1 << 28)
#define MAX_THREAD_NUM 40
#define USE_MOODY_TOKEN 1

void simpleTest() {
    boost::lockfree::queue<int> q;
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    for (int i = 0; i < MAX_PUSH_COUNT; i++)
        q.push(i);
    struct timeval end;
    gettimeofday(&end, nullptr);
    std::cout << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec) << std::endl;
}

std::stringstream *output;

struct param_struct {
    int tid;
    int nid;
    boost::lockfree::queue<int> *q;
};

void *simpleWorker(void *args) {
    struct param_struct *psp = (struct param_struct *) args;
    int tid = psp->tid;
    int nid = psp->nid;
    struct timeval begin;
    gettimeofday(&begin, nullptr);
    unsigned long counter = 0;
    for (int i = 0; i < MAX_PUSH_COUNT / nid; i++) {
        psp->q->push(i);
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
    /*simpleTest();*/
    for (int t = 1; t <= MAX_THREAD_NUM; t++) {
        boost::lockfree::queue<int> q;
        struct param_struct ps;
        ps.q = &q;
        ps.nid = t;
        concurrentTest(ps);
        int v;
        int counter = 0;
        struct timeval begin;
        gettimeofday(&begin, nullptr);
        while (ps.q->pop(v)) counter++;
        struct timeval end;
        gettimeofday(&end, nullptr);
        std::cout << t << " " << counter << " " << ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec)
                  << std::endl;
    }
    return 0;
}