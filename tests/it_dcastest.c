//
// Created by Michael on 2018/12/10.
//
#include "lf_basic.h"
#include "internal.h"

#define DEFAULT_ARRAY_SIZE (1LL << 30)

#define MINIMAL_ARRAY_GRAN 9

struct ab {
    int count;
    char name;
    char level;
    int/*long long*/ total;
    //long long value;
    int a[MINIMAL_ARRAY_GRAN];
};

struct dw {
    long long x;
    long long y;
};

int main() {
    {
        struct ab *x = aligned_malloc(sizeof(struct ab), LF_ALIGN_DOUBLE_POINTER);
        x->count = -1;
        x->name = 'x';
        x->level = 4;
        x->total = -1;
        //x->value = -1;
        for (int i = 0; i < MINIMAL_ARRAY_GRAN; i++) {
            x->a[i] = 4;
        }
        void *px = x;
        char sx[100];
        sprintf(sx, "%llu\0", px);
        struct ab *y = aligned_malloc(sizeof(struct ab), LF_ALIGN_DOUBLE_POINTER);
        y->count = -2;
        y->name = 'y';
        y->level = 3;
        y->total = -2;
        //y->value = -2;
        for (int i = 0; i < MINIMAL_ARRAY_GRAN; i++) {
            y->a[i] = 10;
        }
        atom_t z = 23;
        void *py = y;
        char sy[100];
        sprintf(sy, "%llu\0", py);
        void *pz = abstraction_dcas(x, y, y);
        x->a[0] = -4;
        y->a[0] = -10;
        int vx = x->a[10];
        int vy = y->a[10];
        px = x;
        py = y;
    }
    {
        int *r = aligned_malloc(sizeof(int), LF_ALIGN_DOUBLE_POINTER);
        *r = 4;
        int *s = aligned_malloc(sizeof(int), LF_ALIGN_DOUBLE_POINTER);
        *s = 10;
        int *t = aligned_malloc(sizeof(int), LF_ALIGN_DOUBLE_POINTER);
        *t = 23;
        int vr = *r;
        int vs = *s;
        int vt = *t;
        void *pz = abstraction_dcas(r, t, t);
        int gr = *r;
        int gs = *s;
        int gt = *t;
        /*void *px1 = &x;
        void *py1 = &y;*/
        printf("Dcas pass.\n");
    }
    {
        atom_t p = 4;
        atom_t q = 10;
        q = (atom_t) __sync_val_compare_and_swap(&p, p, q);
        /*px = &x;*/
        atom_t x1 = 4;
        atom_t y1 = 10;
        y1 = abstraction_cas(&x1, y1, x1);
        printf("Cas pass.\n");
    }
    int *array = malloc(sizeof(int) * DEFAULT_ARRAY_SIZE);
    long long *swaps = malloc(sizeof(long long) * DEFAULT_ARRAY_SIZE);
    int *_swap = malloc(sizeof(int) * DEFAULT_ARRAY_SIZE);
    struct timeval tpstart, tpend;
    unsigned long long int count = 0;
    gettimeofday(&tpstart, NULL);
    for (long long i = 0; i < DEFAULT_ARRAY_SIZE; i++) {
        array[i] = i;
        swaps[i] = i;
        _swap[i] = i;
        count++;
    }
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    printf("Init: %d %llu %lf\n", sizeof(count), count, timeused);
    count = 0;
    gettimeofday(&tpstart, NULL);
    for (long long i = 0; i < DEFAULT_ARRAY_SIZE - 1; i += 2) {
        int tmp = array[i];
        array[i] = array[i + 1];
        array[i + 1] = tmp;
        count++;
    }
    gettimeofday(&tpend, NULL);
    timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    printf("Swap: %d %llu %lf\n", sizeof(*array), count, timeused);
    count = 0;
    gettimeofday(&tpstart, NULL);
    for (long long i = 0; i < DEFAULT_ARRAY_SIZE - 1; i += 2) {
        swaps[i + 1] = abstraction_cas((volatile atom_t *) &swaps[i], swaps[i + 1], swaps[i]);
        count++;
    }
    gettimeofday(&tpend, NULL);
    timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    printf("cas: %d %llu %lf\n", sizeof(*swaps), count, timeused);
    /*for (int i = 0; i < 10; i++) {
        printf("%d %d %d\n", i, array[i], swaps[i]);
    }*/
    count = 0;
    gettimeofday(&tpstart, NULL);
    for (long long i = 0; i < DEFAULT_ARRAY_SIZE - 1; i += 2) {
        _swap[i + 1] = __sync_val_compare_and_swap(&_swap[i], _swap[i], _swap[i + 1]);
        count++;
    }
    gettimeofday(&tpend, NULL);
    timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    printf("_cas: %d %llu %lf\n", sizeof(*_swap), count, timeused);
    for (int i = 0; i < 10; i++) {
        printf("%d %d %d %d\n", i, array[i], swaps[i], _swap[i]);
    }

    free(array);
    free(swaps);
    free(_swap);

    gettimeofday(&tpstart, NULL);
    struct dw *abwap = malloc(sizeof(struct dw) * DEFAULT_ARRAY_SIZE);
    for (long long i = 0; i < DEFAULT_ARRAY_SIZE; i++) {
        abwap[i].x = i;
        abwap[i].y = i + 1;
    }
    gettimeofday(&tpend, NULL);
    timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    printf("Init dw: %d %llu %lf\n", sizeof(*abwap), count, timeused);

    for (int i = 0; i < 10; i++) {
        printf("%d %lld %lld %lld\n", i, abwap[i].x, abwap[i].y);
    }

    gettimeofday(&tpstart, NULL);
    for (long long i = 0; i < DEFAULT_ARRAY_SIZE - 1; i += 2) {
        abstraction_dcas(&abwap[i], &abwap[i], &abwap[i + 1]);
        count++;
    }
    gettimeofday(&tpend, NULL);
    timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    printf("Dswap dw: %d %llu %lf\n", sizeof(*abwap), count, timeused);

    for (int i = 0; i < 10; i++) {
        printf("%d %lld %lld %lld\n", i, abwap[i].x, abwap[i].y);
    }
    return 0;
}
