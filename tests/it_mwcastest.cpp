//
// Created by Michael on 2018/12/17.
//

#include <stdio.h>
#include "rel_ptr.h"

int main(int argc, char **argv) {
    unsigned long long v = 1;
    unsigned long long v1 = 1;
    unsigned long long v2 = 2;
    unsigned long long v3 = 3;

#ifdef linux
    #include <stdatomic.h>
    __sync_fetch_and_add(v, 1);
    __sync_val_compare_and_swap(v1, v2, v3);
#else
#if defined(__MINGW64__) || defined(__CYGWIN__)
    unsigned long long u = __sync_fetch_and_add(&v, 1);
    printf("%d %d\n", u, v);
    __sync_val_compare_and_swap(&v1, v2, v3);
    printf("%d %d %d %d\n", v, v1, v2, v3);
    __sync_val_compare_and_swap(&v1, v1, v3);
#else
#include <process.h>
#include <windows.h>
    InterlockedIncrement(&v);
    InterlockedCompareExchange(&v1, v2, v3);
#endif
#endif
    printf("%d %d %d %d\n", v, v1, v2, v3);
    rel_ptr<int> key();
    return 0;
}
