//
// Created by lwh on 19-6-5.
//

#include <iostream>
#include <sstream>
#include "LFResizableHashSet.h"

#define TOTAL_COUNT   (1 << 20)

#define MALLOC_GRAN   (1 << 7)

#define THREAD_NUBMER 4

size_t default_size = MALLOC_GRAN;

long malloc_time;

long free_time;

long write_time;

uint64_t malloc_count = 0;

uint64_t free_count = 0;

uint64_t write_count = 0;

uint64_t total_count = TOTAL_COUNT;

int thread_number = THREAD_NUBMER;

stringstream *output;

atomic<int> stopMeasure(0);

size_t *size;

int random_size = 1;

double avgsize;

struct target {
    int tid;
    char **mm;
};

void *measureWorker(void *args) {
    Tracer tracer;
    struct target *work = (struct target *) args;
    uint64_t hit = 0;
    uint64_t pin = 0;
    uint64_t fail = 0;
    long malloc_elipsed = 0;
    long free_elipsed = 0;
    long write_elipsed = 0;
    while (stopMeasure.load(memory_order_relaxed) == 0) {
        tracer.startTime();
        if (random_size & 0x1 != 0) {
            for (int i = 0; i < total_count / thread_number; i++) {
                work->mm[i] = (char *) malloc(size[i]);
                hit++;
            }
        } else {
            for (int i = 0; i < total_count / thread_number; i++) {
                work->mm[i] = (char *) malloc(default_size);
                hit++;
            }
        }
        malloc_elipsed += tracer.getRunTime();
        tracer.startTime();
        if (random_size & 0x2 != 0) {
            if (random_size & 0x1 != 0) {
                for (int i = 0; i < total_count / thread_number; i++) {
                    memset(work->mm[i], i, size[i]);
                    pin++;
                }
            } else {
                for (int i = 0; i < total_count / thread_number; i++) {
                    memset(work->mm[i], i, default_size);
                    pin++;
                }
            }
        }
        write_elipsed += tracer.getRunTime();
        tracer.startTime();
        for (int i = 0; i < total_count / thread_number; i++) {
            free(work->mm[i]);
            fail++;
        }
        free_elipsed += tracer.getRunTime();
    }
    output[work->tid] << work->tid << " " << malloc_elipsed << " " << free_elipsed << " "
                      << malloc_elipsed + free_elipsed << " " << write_elipsed << endl;

    __sync_fetch_and_add(&malloc_time, malloc_elipsed);
    __sync_fetch_and_add(&write_time, write_elipsed);
    __sync_fetch_and_add(&free_time, free_elipsed);
    __sync_fetch_and_add(&malloc_count, hit);
    __sync_fetch_and_add(&write_count, pin);
    __sync_fetch_and_add(&free_count, fail);
}

void multiWorkers() {
    pthread_t workers[thread_number];
    struct target parms[thread_number];
    cout << "Measuring count: " << total_count << " thread: " << thread_number << " gran: " << avgsize << endl;
    Timer timer;
    timer.start();
    for (int i = 0; i < thread_number; i++) {
        parms[i].tid = i;
        parms[i].mm = (char **) malloc(total_count / thread_number * sizeof(char *));
        pthread_create(&workers[i], nullptr, measureWorker, &parms[i]);
    }
    while (timer.elapsedSeconds() < default_timer_range) {
        sleep(1);
    }
    stopMeasure.store(1, memory_order_relaxed);
    for (int i = 0; i < thread_number; i++) {
        pthread_join(workers[i], nullptr);
        free(parms[i].mm);
        string outstr = output[i].str();
        cout << outstr;
    }
    cout << "Gathering ..." << endl;
}

void generate_size() {
    avgsize = .0f;
    size = (size_t *) malloc(sizeof(size_t) * total_count / thread_number);
    UniformGen<size_t>::generate(size, total_count / thread_number, default_size * 2);
    /*std::default_random_engine engine(static_cast<int>(chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<size_t> dis(0, default_size * 2);*/
    for (size_t i = 0; i < total_count / thread_number; i++) {
        //size[i] = static_cast<size_t>(dis(engine));
        avgsize += size[i];
    }
    avgsize /= (total_count / thread_number);
}

int main(int argc, char **argv) {
    if (argc == 5) {
        cout << "Parameters " << argc << endl;
        thread_number = atoi(argv[1]);
        total_count = atoi(argv[2]);
        random_size = atoi(argv[3]);
        default_size = atoi(argv[4]);
    } else if (argc != 1) {
        cout << "Command thread_number total_count random_size default_size " << endl;
        cout << "\t random_size: 0 (fixed nowrite), 1 (variant nowrite), 2 (fixed write), 3 (variant write)" << endl;
        exit(0);
    }
    avgsize = default_size;
    output = new stringstream[thread_number];
    if (random_size & 0x1 != 0) generate_size();
    Tracer tracer;
    tracer.startTime();
    multiWorkers();
    long ut = tracer.getRunTime();
    cout << " Total " << ut << " malloc count " << malloc_count << " free count " << free_count
         << " malloc avgtpt " << (double) malloc_count * 1000000 * thread_number / malloc_time
         << " free avgtpt " << (double) free_count * 1000000 * thread_number / free_time
         << " total avgtpt " << (double) (malloc_count) * 1000000 * thread_number / (malloc_time + free_time)
         << " write avgtpt " << (double) (write_count) * 1000000 * thread_number / (write_time) << endl;
    delete[] output;
}