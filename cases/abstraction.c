#include "abstraction.h"

unsigned int abstraction_cpu_count(void) {
    unsigned int cpu_count;
    //cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_count = 4;
    return ((unsigned int)cpu_count);
}

int abstraction_thread_start(thread_state_t *thread_state, unsigned int cpu, thread_function_t thread_function, void *thread_user_state) {
    int rv = 0, rv_create;
    pthread_attr_t attr;
#if (defined UNIX)
    cpu_set_t cpuset;
#endif
    assert(thread_state != NULL);
    assert(thread_function != NULL);

    pthread_attr_init(&attr);
#if (defined UNIX)
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);
#endif
    rv_create = pthread_create(thread_state, &attr, thread_function, thread_user_state);
    if (rv_create == 0)
        rv = 1;
    pthread_attr_destroy(&attr);
    return (rv);
}

void abstraction_thread_wait(thread_state_t thread_state) {
    pthread_join(thread_state, NULL);
    return;
}
