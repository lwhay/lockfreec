//
// Created by lwh on 19-3-11.
//

#ifndef MACHINECONSTANTS_H
#define MACHINECONSTANTS_H

#ifndef MAX_THREADS_POW2
#define MAX_THREADS_POW2 128 // MUST BE A POWER OF TWO, since this is used for some bitwise operations
#endif
#ifndef LOGICAL_PROCESSORS
#define LOGICAL_PROCESSORS MAX_THREADS_POW2
#endif

#ifndef SOFTWARE_BARRIER
#   define SOFTWARE_BARRIER asm volatile("": : :"memory")
#endif

// the following definition is only used to pad data to avoid false sharing.
// although the number of words per cache line is actually 8, we inflate this
// figure to counteract the effects of prefetching multiple adjacent cache lines.
#define PREFETCH_SIZE_WORDS 16
#define PREFETCH_SIZE_BYTES 128
#define BYTES_IN_CACHE_LINE 64

#define CAT2(x, y) x##y
#define CAT(x, y) CAT2(x, y)

#define PAD64 volatile char CAT(___padding, __COUNTER__)[64]
#define PAD volatile char CAT(___padding, __COUNTER__)[128]

#define CASB __sync_bool_compare_and_swap
#define CASV __sync_val_compare_and_swap
#define FAA __sync_fetch_and_add

#ifdef __APPLE__

#ifdef __PSEUDO_NUMA__
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/mach_port.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#define SYSCTL_CORE_COUNT   "machdep.cpu.core_count"

typedef struct cpu_set {
    uint32_t count;
} cpu_set_t;

static inline void CPU_ZERO(cpu_set_t *cs) {
    cs->count = 0;
}

static inline void
CPU_SET(int num, cpu_set_t *cs) { cs->count |= (1 << num); }

static inline int
CPU_ISSET(int num, cpu_set_t *cs) { return (cs->count & (1 << num)); }

int sched_getaffinity(pid_t pid, size_t cpu_size, cpu_set_t *cpu_set) {
    int32_t core_count = 0;
    size_t len = sizeof(core_count);
    int ret = sysctlbyname(SYSCTL_CORE_COUNT, &core_count, &len, 0, 0);
    if (ret) {
        printf("error while get core count %d\n", ret);
        return -1;
    }
    cpu_set->count = 0;
    for (int i = 0; i < core_count; i++) {
        cpu_set->count |= (1 << i);
    }

    return 0;
}

int pthread_setaffinity_np(pthread_t thread, size_t cpu_size, cpu_set_t *cpu_set) {
    thread_port_t mach_thread;
    int core = 0;

    for (core = 0; core < 8 * cpu_size; core++) {
        if (CPU_ISSET(core, cpu_set)) break;
    }
    printf("binding to core %d\n", core);
    thread_affinity_policy_data_t policy = {core};
    mach_thread = pthread_mach_thread_np(thread);
    thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY,
                      (thread_policy_t) &policy, 1);
    return 0;
}
#endif
#define PTHREAD_BARRIER_SERIAL_THREAD   1

typedef struct pthread_barrier {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    volatile uint32_t flag;
    size_t count;
    size_t num;
} pthread_barrier_t;


int pthread_barrier_init(pthread_barrier_t *bar, int attr, int num) {
    int ret = 0;
    if ((ret = pthread_mutex_init(&(bar->mutex), 0)))
        return ret;
    if ((ret = pthread_cond_init(&(bar->cond), 0)))
        return ret;
    bar->flag = 0;
    bar->count = 0;
    bar->num = num;
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t *bar) {
    int ret = 0;
    uint32_t flag = 0;

    if ((ret = pthread_mutex_lock(&(bar->mutex))))
        return ret;

    flag = bar->flag;
    bar->count++;

    if (bar->count == bar->num) {
        bar->count = 0;
        bar->flag = 1 - bar->flag;
        if ((ret = pthread_cond_broadcast(&(bar->cond))))
            return ret;
        if ((ret = pthread_mutex_unlock(&(bar->mutex))))
            return ret;
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }

    while (1) {
        if (bar->flag == flag) {
            ret = pthread_cond_wait(&(bar->cond), &(bar->mutex));
            if (ret)
                return ret;
        } else {
            break;
        }
    }

    if ((ret = pthread_mutex_unlock(&(bar->mutex))))
        return ret;
    return 0;
}

#endif

#endif /* MACHINECONSTANTS_H */