//
// Created by Michael on 2019/2/15.
//

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "bloom_hashfunc.h"

#define TOTAL_BITS (1LLU << 32)
#define TOTAL_SETS (1LLU << 28)
#define THREAD_NUM (5)

struct args_struct {
    bitvector_t *bv;
    size_t *bits;
};

void *xorworker(void *args) {
    struct args_struct *as = (struct args_struct *) args;
    for (int i = 0; i < TOTAL_SETS; i++) {
        bv_toggle(as->bv, as->bits[i]);
    }
}

void thread_bitsets() {
    struct args_struct *args = malloc(sizeof(struct args_struct));
    args->bv = bv_create(TOTAL_BITS);
    args->bits = malloc(sizeof(size_t) * TOTAL_SETS);
    size_t set_before = 0;
    for (size_t i = 0; i < TOTAL_BITS; i++) {
        if (bv_get(args->bv, i)) {
            set_before++;
        }
    }
    for (int i = 0; i < TOTAL_SETS; i++) {
        args->bits[i] = TOTAL_BITS / TOTAL_SETS * i;
    }
    pthread_t threads[THREAD_NUM];
    struct timeval begin;
    gettimeofday(&begin, NULL);
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_create(&threads[i], NULL, xorworker, args);
    }
    for (int i = 0; i < THREAD_NUM; i++) {
        pthread_join(threads[i], NULL);
    }
    struct timeval end;
    gettimeofday(&end, NULL);
    size_t set_after = 0;
    for (size_t i = 0; i < TOTAL_BITS; i++) {
        if (bv_get(args->bv, i)) {
            set_after++;
        }
    }
    printf("%llu, %llu, %llu, %llu, %d\n", TOTAL_BITS, TOTAL_SETS, set_before, set_after,
           ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec));
    bv_destroy(args->bv);
    free(args->bits);
    free(args);
}

int main() {
    thread_bitsets();
}