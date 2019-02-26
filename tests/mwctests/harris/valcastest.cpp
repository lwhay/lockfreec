//
// Created by Michael on 2019-02-26.
//
#include <atomic>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

void naivetest() {
    printf("*****************************\n");
    int a = 1;
    int b = 2;
    int *pa = &a;

    printf("%d, %d\n", __sync_val_compare_and_swap(pa, b, a), a);
    printf("%d, %d\n", __sync_val_compare_and_swap(pa, a, b), a);

    int aa = 1;
    int ab = 2;
    std::atomic<int> aaa(aa);
    int oa = aaa.load();
    aaa.compare_exchange_weak(ab, ab);
    // Failure without changing oa, while the first ab is changed;
    // Equal case 1: see the first ab <- aaa.load()
    printf("%d, %d, %d\n", oa, aa, ab);
    oa = aaa.load();
    aaa.compare_exchange_weak(aa, ab);
    // Success changing oa by ab's new value, i.e., 1;
    printf("%d, %d, %d\n", oa, aa, ab);
    ab = 2;
    oa = aaa.load();
    aaa.compare_exchange_weak(aa, ab);
    int na = aaa.load();
    // Success without changing oa;
    // Equal case 2: see aa <- aaa's old value.
    printf("%d, %d, %d, %d\n", oa, aa, na, ab);
    printf("*****************************\n");
}

int oval = 1;
int nval = 2;
std::atomic<int> pto(oval);
std::atomic<size_t> ttick(0);
std::atomic<size_t> itick(0);

pthread_mutex_t mutex;
pthread_mutex_t begin;

void *tryWorker(void *args) {
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&begin);
    int oldv = pto.load();
    int ov = 1;
    do {
        oldv = pto.load(); // If inferencer changed oldv beforehand, we have oldv by 2.
        ttick.fetch_add(1);
    } while (pto.compare_exchange_weak(ov, oval)); // We expect a failure, where pto is changed yet oldv is not.
    printf("\t%d, %d\n", oldv, pto.load());
    pthread_mutex_unlock(&mutex);
    printf("\tTrier exits with ov: %d.\n", ov); // Always fails in CAS.
}

void *inferencer(void *args) {
    int ov = 1;
    pthread_mutex_lock(&begin);
    do {
        itick.fetch_add(1);
        pto.compare_exchange_weak(ov, nval);
        if (pthread_mutex_trylock(&mutex) != EBUSY) {
            break;
        }
    } while (true);
    printf("\tInfer exits with ov: %d.\n", ov); // Always fails in CAS.
}

// Can we atomically realize __sync_val_compare_and_swap by std::atomic?
// We can use the first parameter of compare_exchange_weak if we get success.
// Notice compare_exchange_weak/compare_exchange_strong, except for spurious fails, both
// compares the contents of the atomic object's contained value with expected:
//  - if true, it replaces the contained value with val (like store).
//  - if false, it replaces #expected with the contained value .
void inference() {
    for (int i = 0; i < 100; i++) {
        //pto.store(1); // Instead, we can use the following two lines.
        oval = 1;
        pto = oval;
        ttick = 0;
        itick = 0;
        printf("Round %d:\n", i);
        pthread_t tWorker, iWorker;
        pthread_mutex_init(&mutex, nullptr);
        pthread_mutex_init(&begin, nullptr);
        pthread_mutex_lock(&begin);
        pthread_create(&tWorker, nullptr, tryWorker, nullptr);
        pthread_create(&iWorker, nullptr, inferencer, nullptr);
        pthread_join(tWorker, nullptr);
        pthread_join(iWorker, nullptr);
        printf("\ttick: %llu, itick: %llu\n", ttick.load(), itick.load());
        pthread_mutex_destroy(&mutex);
        pthread_mutex_destroy(&begin);
    }
}

int main() {
    naivetest();
    inference();
    naivetest();
    return 0;
}