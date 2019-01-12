#ifndef ABSTRACTION_H
#define ABSTRACTION_H
#if (defined __GNUC__ && defined __x86_64__)

#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

typedef pthread_t thread_state_t;
typedef void *thread_return_t;
#define CALLING_CONVENTION
#endif

typedef thread_return_t (CALLING_CONVENTION *thread_function_t)(void *thread_user_state);

unsigned int abstraction_cpu_count(void);

int abstraction_thread_start(thread_state_t *thread_state, unsigned int cpu, thread_function_t thread_function,
                             void *thread_user_state);

void abstraction_thread_wait(thread_state_t thread_state);

#endif // ABSTRACTION_H
