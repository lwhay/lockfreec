//
// Created by Michael on 2018/12/20.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "ebr.h"

#if defined(linux) || defined(__MINGW64__) || defined(__CYGWIN__) || defined(__APPLE__)

#include <pthread.h>

typedef pthread_t HANDLE;
#else
#endif

#define PARALLEL_DEGREE     4

#define TOTAL_NUM           (1 << 10)

static pthread_cond_t cond;
static pthread_mutex_t lock;

bool complete = false;

void *monitor(void *args) {
    while (!complete) {
        if (1) {
            /*if (ebr_sync(local_ebr, &local_ebr->local_epoch)) {

            }*/
            pthread_mutex_unlock(&lock);
            //pthread_cond_broadcast(&cond);
            pthread_cond_signal(&cond);
        }
    }
}

void *worker(void *args) {
    ebr_t **cache = new ebr_t *[TOTAL_NUM];
    unsigned long long cur = 0;
    unsigned long long rec = 0;
    ebr_register(cache[rec]);
    for (int i = 0; i < TOTAL_NUM; i++) {
        //if (i % (TOTAL_NUM / PARALLEL_DEGREE) == 0) {

        //}
        if (i % PARALLEL_DEGREE != PARALLEL_DEGREE - 1) {
            cache[rec] = ebr_create();
            if (cache[rec] == nullptr)
                printf("*");
            else
                printf(".");
            ebr_enter(cache[rec++]);
        } else {
            ebr_exit(cache[cur]);
            ebr_destroy(cache[cur++]);
        }
    }
    printf("\n%d %d\n", cur, rec);
    while (cur < rec) {
        //ebr_exit(cache[cur]);
        ebr_destroy(cache[cur++]);
    }
    delete[] cache;
}

int main(int argc, char **argv) {
    printf("%d %d %d %d\n", sizeof(NULL), sizeof(nullptr), (NULL == nullptr), (1 == 1));
    HANDLE *threads = new HANDLE[PARALLEL_DEGREE];
    int err;
    pthread_cond_init(&cond, NULL);
    for (int i = 0; i < PARALLEL_DEGREE; i++) {
        if (err = pthread_create(threads + i, nullptr, worker, nullptr)) {
            printf("Error %d on creating %d\n", err, i);
        }
    }
    for (int i = 0; i < PARALLEL_DEGREE; i++) {
        pthread_join(threads[i], nullptr);
    }
    printf("Joined workers\n");
    unsigned long long counter = 0;
    while (local_ebr != nullptr && local_ebr->next != nullptr) {
        counter++;
        if (counter > TOTAL_NUM) {
            printf("+");
        }
    }
    printf("\n%lld", counter);
    pthread_mutex_lock(&lock);
    HANDLE mon;
    err = pthread_create(&mon, nullptr, monitor, nullptr);

    /*struct timeval time;
    gettimeofday(&time, nullptr);
    struct timespec outtime;
    outtime.tv_sec = time.tv_sec + 5;
    outtime.tv_nsec = time.tv_usec * 1000000;
    pthread_cond_timedwait(&cond, &lock, &outtime);*/
    pthread_cond_wait(&cond, &lock);
    pthread_mutex_unlock(&lock);

    complete = true;
    pthread_join(mon, nullptr);
    delete[] threads;
    return 0;
}