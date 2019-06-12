//
// Created by Michael on 2018/12/10.
//
#include <iostream>
#include <malloc.h>
#include <assert.h>
#include "tracer.h"

#define DEFAULT_VOLUME (1LL << 32)

#define DEFAULT_GRAN 10

#define DEFAULT_ROUND (DEFAULT_VOLUME / DEFAULT_GRAN + 1)

#define ALIGH_SIZE 8

struct dummy {
    char data[DEFAULT_GRAN];
};

void mallocTest() {
    cout << "Total: " << DEFAULT_VOLUME << " Gran: " << DEFAULT_GRAN << " Round: " << DEFAULT_ROUND << endl;
    Tracer tracer;
    tracer.startTime();
    struct dummy *dp = (struct dummy *) malloc(sizeof(dummy) * DEFAULT_ROUND);
    cout << "\tMalloc batch: " << tracer.getRunTime() << endl;
    tracer.startTime();
    free(dp);
    cout << "\tDealloc batch: " << tracer.getRunTime() << endl;
    struct dummy **dpp = (struct dummy **) malloc(sizeof(dummy *) * DEFAULT_ROUND);
    cout << "\tMalloc encapsulators: " << tracer.getRunTime() << endl;
    tracer.startTime();
    for (int i = 0; i < DEFAULT_ROUND; i++) {
        dpp[i] = (struct dummy *) malloc(sizeof(struct dummy));
    }
    cout << "\tMalloc elements: " << tracer.getRunTime() << endl;
    tracer.startTime();
    for (int i = 0; i < DEFAULT_ROUND; i++) {
        free(dpp[i]);
    }
    cout << "\tDealloc elements: " << tracer.getRunTime() << endl;
    tracer.startTime();
    free(dpp);
    cout << "\tDealloc encapsulators: " << tracer.getRunTime() << endl;
}

#ifdef __MINGW64__
void mmMallocTest() {
    cout << "Total: " << DEFAULT_VOLUME << " Gran: " << DEFAULT_GRAN << " Round: " << DEFAULT_ROUND << endl;
    Tracer tracer;
    tracer.startTime();
    struct dummy *dp = (struct dummy *) _mm_malloc(sizeof(dummy) * DEFAULT_ROUND, ALIGH_SIZE);
    cout << "\tmmMalloc batch: " << tracer.getRunTime() << endl;
    tracer.startTime();
    _mm_free(dp);
    cout << "\tmmFree batch: " << tracer.getRunTime() << endl;
    struct dummy **dpp = (struct dummy **) _mm_malloc(sizeof(dummy *) * DEFAULT_ROUND, ALIGH_SIZE);
    cout << "\tmmMalloc encapsulators: " << tracer.getRunTime() << endl;
    tracer.startTime();
    for (int i = 0; i < DEFAULT_ROUND; i++) {
        dpp[i] = (struct dummy *) _mm_malloc(sizeof(struct dummy), ALIGH_SIZE);
    }
    cout << "\tmmMalloc elements: " << tracer.getRunTime() << endl;
    tracer.startTime();
    for (int i = 0; i < DEFAULT_ROUND; i++) {
        _mm_free(dpp[i]);
    }
    cout << "\tmmFree elements: " << tracer.getRunTime() << endl;
    tracer.startTime();
    _mm_free(dpp);
    cout << "\tmmFree encapsulators: " << tracer.getRunTime() << endl;
}
#endif

void *aligned_malloc(size_t size, size_t align_in_bytes) {
    void *original_memory, *memory;
    size_t offset;
    original_memory = memory = malloc(size + sizeof(void *) + align_in_bytes);
    if (NULL != memory) {
        memory = (void **) memory + 1;
        offset = align_in_bytes - (size_t) memory % align_in_bytes;
        memory = (unsigned char *) memory + offset;
        *((void **) memory - 1) = original_memory;
    }
    return (memory);
}

void aligned_free(void *memory) {
    assert(memory != NULL);
    free(*((void **) memory - 1));
}

void alignMallocTest() {
    cout << "Total: " << DEFAULT_VOLUME << " Gran: " << DEFAULT_GRAN << " Round: " << DEFAULT_ROUND << endl;
    Tracer tracer;
    tracer.startTime();
    struct dummy *dp = (struct dummy *) aligned_malloc(sizeof(dummy) * DEFAULT_ROUND, ALIGH_SIZE);
    cout << "\tAlignMalloc batch: " << tracer.getRunTime() << endl;
    tracer.startTime();
    aligned_free(dp);
    cout << "\tAlignFree batch: " << tracer.getRunTime() << endl;
    struct dummy **dpp = (struct dummy **) aligned_malloc(sizeof(dummy *) * DEFAULT_ROUND, ALIGH_SIZE);
    cout << "\tAlignMalloc encapsulators: " << tracer.getRunTime() << endl;
    tracer.startTime();
    for (int i = 0; i < DEFAULT_ROUND; i++) {
        dpp[i] = (struct dummy *) aligned_malloc(sizeof(struct dummy), ALIGH_SIZE);
    }
    cout << "\tAlignMalloc elements: " << tracer.getRunTime() << endl;
    tracer.startTime();
    for (int i = 0; i < DEFAULT_ROUND; i++) {
        aligned_free(dpp[i]);
    }
    cout << "\tAlignFree elements: " << tracer.getRunTime() << endl;
    tracer.startTime();
    aligned_free(dpp);
    cout << "\tAlignFree encapsulators: " << tracer.getRunTime() << endl;
}

void newTest() {
    cout << "Total: " << DEFAULT_VOLUME << " Gran: " << DEFAULT_GRAN << " Round: " << DEFAULT_ROUND << endl;
    Tracer tracer;
    tracer.startTime();
    struct dummy *dp = new struct dummy[DEFAULT_ROUND];
    cout << "\tNew batch: " << tracer.getRunTime() << endl;
    tracer.startTime();
    delete[] dp;
    cout << "\tDelete batch: " << tracer.getRunTime() << endl;
    struct dummy **dpp = new struct dummy *[DEFAULT_ROUND];
    cout << "\tNew encapsulators: " << tracer.getRunTime() << endl;
    tracer.startTime();
    for (int i = 0; i < DEFAULT_ROUND; i++) {
        dpp[i] = new struct dummy;
    }
    cout << "\tNew elements: " << tracer.getRunTime() << endl;
    tracer.startTime();
    for (int i = 0; i < DEFAULT_ROUND; i++) {
        delete dpp[i];
    }
    cout << "\tDelete elements: " << tracer.getRunTime() << endl;
    tracer.startTime();
    delete[] dpp;
    cout << "\tDelete encapsulators: " << tracer.getRunTime() << endl;
}

int main(int argc, char **argv) {
    mallocTest();
#ifdef __MINGW64__
    mmMallocTest();
#endif
    alignMallocTest();
    newTest();
    return 0;
}