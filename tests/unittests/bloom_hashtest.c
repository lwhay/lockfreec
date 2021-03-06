//
// Created by Michael on 2019/2/15.
//

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "bloom_hashfunc.h"

#define TOTAL_BITS (1LLU << 24)
#define TOTAL_SETS (1LLU << 24)
#define ERROR_RATE (0.01)
#define THREAD_NUM (4)
#define CONCURRENT (0)
#define STATICHASH (1)

struct args_struct {
#if STATICHASH
    size_t *hc;
#endif
    bloom_t *bh;
    size_t *bits;
    size_t false_negatives;
};

void *setworker(void *args) {
    struct args_struct *as = (struct args_struct *) args;
    for (size_t i = 0; i < TOTAL_SETS; i++) {
        if (i % (1 << 20) == 0) {
            printf("%llu\t%llu\n", i / (1 << 20), i);
        }
#if STATICHASH
        for (int j = 0; j < as->bh->nhashes; j++) {
            as->hc[j] = 0;
        }
        bloom_attach(as->bh, (unsigned char *) &((as->bits[i])), sizeof(size_t), as->hc);
#else
        bloom_add(as->bh, (unsigned char *) &((as->bits[i])), sizeof(size_t));
#endif
    }
}

void *getworker(void *args) {
    struct args_struct *as = (struct args_struct *) args;
    for (int i = 0; i < TOTAL_SETS; i++) {
#if STATICHASH
        for (int j = 0; j < as->bh->nhashes; j++) {
            as->hc[j] = 0;
        }
        if (!bloom_obtain(as->bh, (unsigned char *) &((as->bits[i])), sizeof(size_t), as->hc)) {
#else
            if (!bloom_get(as->bh, (unsigned char *) &((as->bits[i])), sizeof(size_t))) {
#endif
            as->false_negatives++;
        }
    }
}

void thread_bloomhash() {
    struct args_struct *args = malloc(sizeof(struct args_struct) * THREAD_NUM);
#if CONCURRENT
    bloom_t *bh = bloom_create(TOTAL_BITS, ERROR_RATE);
#endif
    for (int k = 0; k < THREAD_NUM; k++) {
#if CONCURRENT
        args[k].bh = bh;
#else
        args[k].bh = bloom_create(TOTAL_BITS, ERROR_RATE);
#if STATICHASH
        args[k].hc = calloc(args[k].bh->nhashes, sizeof(size_t));
#endif
#endif
        args[k].bits = malloc(sizeof(size_t) * TOTAL_SETS);
        for (int i = 0; i < TOTAL_SETS; i++) {
            args->bits[i] = TOTAL_BITS / TOTAL_SETS * i + k;
        }
    }
    printf("Ready!\n");
    pthread_t threads[THREAD_NUM];
    struct timeval begin;
    gettimeofday(&begin, NULL);
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_create(&threads[i], NULL, setworker, args + i);
    }
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(threads[i], NULL);
    }
    struct timeval end;
    gettimeofday(&end, NULL);

    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_create(&threads[i], NULL, getworker, args + i);
    }
    size_t false_negatives = 0;
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(threads[i], NULL);
        false_negatives += args[i].false_negatives;
    }
    size_t false_positives = 0;
    for (int k = 0; k < THREAD_NUM; k++) {
        for (int i = 0; i < TOTAL_SETS; i++) {
            size_t positives = TOTAL_BITS + i;
#if STATICHASH
            for (int j = 0; j < args[k].bh->nhashes; j++) {
                args[k].hc[j] = 0;
            }
            if (bloom_obtain(args[k].bh, (unsigned char *) &positives, sizeof(size_t), args[k].hc)) {
#else
                if (bloom_get(args[k].bh, (unsigned char *) &positives, sizeof(size_t))) {
#endif
                false_positives++;
            }
        }
#if STATICHASH
        bloom_destroy(args[k].bh);
        free(args[k].hc);
        free(args[k].bits);
#endif
    }
    printf("%llu, %llu, %llu, %llu, %f, %d\n", TOTAL_BITS, TOTAL_SETS, false_positives, false_negatives,
           (double) false_positives / (TOTAL_SETS * THREAD_NUM),
           ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec));
#if !(STATICHASH)
    bloom_destroy(args->bh);
    free(args->bits);
#endif
    free(args);
}

int main() {
    thread_bloomhash();
}