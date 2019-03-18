//
// Created by lwh on 19-3-12.
//


#ifndef _TSC_H_
#define _TSC_H_

#include <stdint.h>

static inline uint64_t
read_tsc(void) {
    unsigned upper, lower;
    asm volatile("rdtsc":"=a"(lower), "=d"(upper)::"memory");
    return ((uint64_t) lower) | (((uint64_t) upper) << 32);
}

#endif    /*_TSC_H_*/