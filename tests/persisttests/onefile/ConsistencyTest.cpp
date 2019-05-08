//
// Created by Michael on 2019-05-08.
//

#include <iostream>
#include <sstream>
#include "tracer.h"
#include "OneFileLF.h"

#define TOTAL_COUNT   (1 << 20)

#define QUERY_COUNT   (1 << 7)

#define THREAD_NUBMER 1

#define ATOMIC_UNIT   2

#define ATOMIC_FIRST  0

#define ATOMIC_SECOND 1

long total_time;

uint64_t exists = 0;

uint64_t update = 0;

uint64_t failure = 0;

uint64_t total_count = TOTAL_COUNT;

int thread_number = THREAD_NUBMER;

uint64_t *loads;

// The two cases of oflf, saying MEASURE_TYPE == 0, are definitely correct.

oflf::tmtype<uint64_t> *value[ATOMIC_UNIT];

stringstream *logs;

stringstream *output;

struct target {
    int tid;
    oflf::tmtype<uint64_t> *value[ATOMIC_UNIT];
    /*uint64_t get() {
        oflf::readTx([&] {
            uint64_t ret = *value;
            return ret;
        });
    }*/
};

void *measureWorker(void *args) {
    Tracer tracer;
    tracer.startTime();
    struct target *work = (struct target *) args;
    uint64_t hit = 0, trie = 0;
    uint64_t fail = 0;
    float sum = 0.1;
    int tid = work->tid;
    uint64_t final1 = 0, final2 = 0;
    for (int i = 0; i < total_count; i++ /*+= thread_number*/) {
        uint64_t trick = 0;
        oflf::updateTx([&]() {
            fail++;
            uint64_t tmp1 = *work->value[ATOMIC_FIRST];
            uint64_t tmp2 = *work->value[ATOMIC_SECOND];
            //logs[tid] << "\t" << tid << " " << trie++ << " " << trick++ << " " << tmp1 << " " << tmp2 << endl;
            assert(tmp1 == tmp2);
            tmp1 += 1;
            *work->value[ATOMIC_FIRST] = tmp1;

            tmp2 += 1;
            *work->value[ATOMIC_SECOND] = tmp2;
            //cout << "\t" << tid << " " << tmp << endl;
        });
        //tmp = work->get();
        hit++;
    }
    oflf::readTx([&]() {
        final1 = *work->value[ATOMIC_FIRST];
        final2 = *work->value[ATOMIC_SECOND];
    });
    long elipsed = tracer.getRunTime();
    output[tid] << tid << " " << final1 << " " << final2 << " " << elipsed << " " << sum << " " << endl;
    __sync_fetch_and_add(&total_time, elipsed);
    __sync_fetch_and_add(&update, hit);
    __sync_fetch_and_add(&failure, fail);
}

void multiWorkers() {
    pthread_t workers[thread_number];
    struct target **parms = (struct target **) malloc(sizeof(struct target *) * thread_number);
    cout << "Measuring ..." << endl;
    oflf::gOFLF.debug = false;
    for (int i = 0; i < thread_number; i++) {
        parms[i] = new struct target;
        parms[i]->tid = i;
        for (int j = 0; j < ATOMIC_UNIT; j++) {
            parms[i]->value[j] = value[j];
        }
        pthread_create(&workers[i], nullptr, measureWorker, parms[i]);
    }
    for (int i = 0; i < thread_number; i++) {
        pthread_join(workers[i], nullptr);
        string outstr = output[i].str();
        cout << outstr;
        outstr = logs[i].str();
        cout << outstr;
    }
    oflf::gOFLF.debug = false;
    cout << "Gathering ..." << endl;
    delete[] parms;
}

int main(int argc, char **argv) {
    if (argc == 3) {
        cout << "Parameters " << argc << endl;
        thread_number = atoi(argv[1]);
        total_count = atoi(argv[2]);
    }
    output = new stringstream[thread_number];
    logs = new stringstream[thread_number];
    loads = new uint64_t[total_count];
    for (int i = 0; i < total_count; i++) {
        loads[i] = i;
    }
    for (int i = 0; i < ATOMIC_UNIT; i++) {
        value[i] = (oflf::tmtype<uint64_t> *) oflf::tmMalloc(sizeof(oflf::tmtype<uint64_t>));
    }
    //value = oflf::tmNew<oflf::tmtype<uint64_t>>();
    Tracer tracer;
    tracer.startTime();
    multiWorkers();
    long ut = tracer.getRunTime();
    cout << "ut " << ut << " dupinst " << exists << " tryupd " << update << " failinst " << failure << " avgtpt "
         << (double) update * 1000000 * thread_number / total_time << endl;
    for (int i = 0; i < ATOMIC_UNIT; i++) {
        oflf::tmFree(value[i]);
    }
    delete[] loads;
    delete[] output;
    delete[] logs;
}