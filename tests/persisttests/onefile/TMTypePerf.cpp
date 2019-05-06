//
// Created by lwh on 19-5-6.
//

#include <iostream>
#include <sstream>
#include "tracer.h"

#define TOTAL_COUNT   (1 << 20)

#define QUERY_COUNT   (1 << 7)

#define THREAD_NUBMER 8

#define MEASURE_TYPE  0 // 0: oflf, 2: poflf, 4: none

long total_time;

uint64_t exists = 0;

uint64_t update = 0;

uint64_t failure = 0;

uint64_t total_count = TOTAL_COUNT;

int thread_number = THREAD_NUBMER;

uint64_t *loads;

// The two cases of oflf, saying MEASURE_TYPE == 0, are definitely correct.
#if (MEASURE_TYPE == 0)
#define ISOLATION  0

#include "OneFileLF.h"

#if ISOLATION
oflf::tmtype<uint64_t> value;
#else
oflf::tmtype<uint64_t> *value;
#endif
#elif (MEASURE_TYPE == 2)

#include "OneFilePTMLF.h"

poflf::tmtype<uint64_t> palue;
#else
uint64_t ralue;
#endif

stringstream *output;

struct target {
    int tid;
#if (MEASURE_TYPE == 0)
#if ISOLATION
    oflf::tmtype<uint64_t> value;
#else
    oflf::tmtype<uint64_t> *value;
#endif
#elif (MEASURE_TYPE == 2)
    poflf::tmtype<uint64_t> *palue;
#else
    uint64_t *ralue;
#endif
};

void *measureWorker(void *args) {
    Tracer tracer;
    tracer.startTime();
    struct target *work = (struct target *) args;
    uint64_t hit = 0;
    uint64_t fail = 0;
    for (int i = work->tid; i < total_count; i++) {
#if (MEASURE_TYPE == 0)
#if ISOLATION
        oflf::updateTx([&]() { work->value = loads[i]; });
#else
        oflf::updateTx([&]() { *work->value = loads[i]; });
#endif
        //work->value = loads[i];
#elif (MEASURE_TYPE == 2)
        poflf::updateTx([&]() { *work->palue = loads[i]; });
#else
        *work->ralue = loads[i];
#endif
        hit++;
    }
    long elipsed = tracer.getRunTime();
    output[work->tid] << work->tid << " " << elipsed << " " << endl;
    __sync_fetch_and_add(&total_time, elipsed);
    __sync_fetch_and_add(&update, hit);
    __sync_fetch_and_add(&failure, fail);
}

void multiWorkers() {
    pthread_t workers[thread_number];
#if (MEASURE_TYPE == 0)
#if ISOLATION
    struct target *parms = (struct target *) oflf::tmMalloc(sizeof(struct target) * thread_number);
#else
    struct target *parms = (struct target *) malloc(sizeof(struct target) * thread_number);
#endif
#elif (MEASURE_TYPE == 2)
    struct target *parms = (struct target *) malloc(sizeof(struct target) * thread_number);
#else
    struct target *parms = (struct target *) malloc(sizeof(struct target) * thread_number);
#endif
    cout << "Measuring ..." << endl;
    //oflf::gOFLF.debug = false;
    for (int i = 0; i < thread_number; i++) {
#if (MEASURE_TYPE == 0)
        //oflf::updateTx([&] {
        parms[i].tid = i;
#if ISOLATION
        parms[i].value = value;
#else
        /*parms[i].value = (oflf::tmtype<uint64_t> *) oflf::tmMalloc(
                sizeof(oflf::tmtype<uint64_t>));*/
        //oflf::tmNew<oflf::tmtype<uint64_t>>();
        parms[i].value = value;
#endif
        //});
        pthread_create(&workers[i], nullptr, measureWorker, &parms[i]);
#elif (MEASURE_TYPE == 2)
        //parms[i] = poflf::tmNew<struct target>();
        //poflf::updateTx([&] {
        parms[i].tid = i;
        parms[i].palue = poflf::tmNew<poflf::tmtype<uint64_t>>();
        //parms[i].palue = palue;
        //});
        pthread_create(&workers[i], nullptr, measureWorker, &parms[i]);
#else
        parms[i].tid = i;
        parms[i].ralue = &ralue;
        pthread_create(&workers[i], nullptr, measureWorker, &parms[i]);
#endif
    }
    for (int i = 0; i < thread_number; i++) {
        pthread_join(workers[i], nullptr);
        string outstr = output[i].str();
        cout << outstr;
    }
    //oflf::gOFLF.debug = false;
    cout << "Gathering ..." << endl;
#if (MEASURE_TYPE == 0)
#if ISOLATION
    oflf::tmFree(parms);
#else
    /*for (int i = 0; i < thread_number; i++) {
        oflf::tmFree(parms[i].value);
    }*/
    delete[] parms;
#endif
#elif (MEASURE_TYPE == 2)
    poflf::tmFree(parms);
#else
    free(parms);
#endif
}

int main(int argc, char **argv) {
    if (argc == 3) {
        cout << "Parameters " << argc << endl;
        thread_number = atoi(argv[1]);
        total_count = atoi(argv[2]);
    }
    output = new stringstream[thread_number];
    loads = new uint64_t[total_count];
    for (int i = 0; i < total_count; i++) {
        loads[i] = i;
    }
#if (MEASURE_TYPE == 0)
#if !ISOLATION
    value = (oflf::tmtype<uint64_t> *) oflf::tmMalloc(sizeof(oflf::tmtype<uint64_t>));
#endif
#endif
    Tracer tracer;
    tracer.startTime();
    multiWorkers();
    long ut = tracer.getRunTime();
    cout << "ut " << ut << " dupinst " << exists << " tryupd " << update << " failinst " << failure << " avgtpt "
         << (double) update * 1000000 * thread_number / total_time << endl;
    delete[] loads;
    delete[] output;
}