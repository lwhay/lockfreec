#include "internal.h"

void test_stack(void) {
    printf("\n"
           "Stack tests\n"
           "===========\n");
    stack_test_internal_popping();
    stack_test_internal_pushing();
    stack_test_internal_popping_and_pushing();
    stack_test_internal_rapid_popping_and_pushing();
    return;
}

void stack_test_internal_popping(void) {
    unsigned int loop, *found_count, cpu_count;
    atom_t count;
    thread_state_t *thread_handles;
    enum data_structure_validity dvs = VALIDITY_VALID;
    struct stack_state *ss;
    struct stack_test_popping_state *stps;

    internal_display_test_name("Popping");

    cpu_count = abstraction_cpu_count();
    stack_new(&ss, 1000000);

    for (loop = 0; loop < 1000000; loop++)
        stack_push(ss, (void *) (atom_t) loop);

    stps = abstraction_malloc(sizeof(struct stack_test_popping_state) * cpu_count);

    for (loop = 0; loop < cpu_count; loop++) {
        (stps + loop)->ss = ss;
        stack_new(&(stps + loop)->ss_thread_local, 1000000);
    }

    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);

    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, stack_test_internal_thread_popping, stps + loop);

    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);

    abstraction_free(thread_handles);
    found_count = abstraction_malloc(sizeof(unsigned int) * 1000000);

    for (loop = 0; loop < 1000000; loop++)
        *(found_count + loop) = 0;

    for (loop = 0; loop < cpu_count; loop++)
        while (stack_pop((stps + loop)->ss_thread_local, (void **) &count))
            (*(found_count + count))++;

    for (loop = 0; loop < 1000000 AND dvs == VALIDITY_VALID; loop++) {
        if (*(found_count + loop) == 0)
            dvs = VALIDITY_INVALID_MISSING_ELEMENTS;
        if (*(found_count + loop) > 1)
            dvs = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
    }

    abstraction_free(found_count);

    for (loop = 0; loop < cpu_count; loop++)
        stack_delete((stps + loop)->ss_thread_local, NULL, NULL);

    abstraction_free(stps);
    stack_delete(ss, NULL, NULL);

    internal_display_test_result(1, "stack", dvs);

    return;
}

thread_return_t CALLING_CONVENTION stack_test_internal_thread_popping(void *state) {
    struct stack_test_popping_state *stps;
    atom_t count;

    assert(NULL != state);

    stps = (struct stack_test_popping_state *) state;
    stack_use(stps->ss);

    while (stack_pop(stps->ss, (void **) &count))
        stack_push(stps->ss_thread_local, (void *) count);

    return ((thread_return_t) EXIT_SUCCESS);
}

void stack_test_internal_pushing(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    enum data_structure_validity dvs[2];
    struct stack_test_pushing_state *stps;
    struct stack_state *ss;
    atom_t data, thread, count, *per_thread_counters;
    struct validation_info vi = {1000000, 1000000};

    internal_display_test_name("Pushing");

    cpu_count = abstraction_cpu_count();
    stps = abstraction_malloc(sizeof(struct stack_test_popping_state) * cpu_count);
    stack_new(&ss, 1000000);

    for (loop = 0; loop < cpu_count; loop++) {
        (stps + loop)->thread_number = (atom_t) loop;
        (stps + loop)->ss = ss;
    }

    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);

    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, stack_test_internal_thread_pushing, stps + loop);

    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);

    abstraction_free(thread_handles);
    per_thread_counters = abstraction_malloc(sizeof(atom_t) * cpu_count);

    for (loop = 0; loop < cpu_count; loop++)
        *(per_thread_counters + loop) = 1000000;

    stack_query(ss, STACK_QUERY_VALIDATE, &vi, (void *) dvs);

    while (dvs[0] == VALIDITY_VALID AND stack_pop(ss, (void **) &data)) {
        thread = data >> (sizeof(atom_t) * 8 - 8);
        count = (data << 8) >> 8;
        if (thread >= cpu_count) {
            dvs[0] = VALIDITY_INVALID_TEST_DATA;
            break;
        }
        if (count > per_thread_counters[thread])
            dvs[0] = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
        if (count < per_thread_counters[thread])
            per_thread_counters[thread] = count - 1;
    }

    abstraction_free(per_thread_counters);
    abstraction_free(stps);
    stack_delete(ss, NULL, NULL);
    internal_display_test_result(2, "stack", dvs[0], "stack freelist", dvs[1]);

    return;
}

thread_return_t CALLING_CONVENTION stack_test_internal_thread_pushing(void *state) {
    struct stack_test_pushing_state *stps;
    atom_t counter = 0;

    assert(NULL != state);

    stps = (struct stack_test_pushing_state *) state;
    stack_use(stps->ss);

    while (stack_push(stps->ss, (void **) ((stps->thread_number << (sizeof(atom_t) * 8 - 8)) | counter++)));

    return ((thread_return_t) EXIT_SUCCESS);
}

void stack_test_internal_popping_and_pushing(void) {
    unsigned int loop, subloop, cpu_count;
    thread_state_t *thread_handles;
    enum data_structure_validity dvs[2];
    struct stack_state *ss;
    struct stack_test_popping_and_pushing_state *stpps;
    struct validation_info vi;

    internal_display_test_name("Popping and pushing (10 seconds)");

    cpu_count = abstraction_cpu_count();
    stack_new(&ss, 100000 * cpu_count * 2);

    for (loop = 0; loop < 100000 * cpu_count; loop++)
        stack_push(ss, (void *) (atom_t) loop);

    stpps = abstraction_malloc(sizeof(struct stack_test_popping_and_pushing_state) * cpu_count * 2);

    for (loop = 0; loop < cpu_count; loop++) {
        (stpps + loop)->ss = ss;
        stack_new(&(stpps + loop)->local_ss, 100000);
        (stpps + loop + cpu_count)->ss = ss;
        stack_new(&(stpps + loop + cpu_count)->local_ss, 100000);

        for (subloop = 0; subloop < 100000; subloop++)
            stack_push((stpps + loop + cpu_count)->local_ss, (void *) (atom_t) subloop);
    }

    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count * 2);

    for (loop = 0; loop < cpu_count; loop++) {
        abstraction_thread_start(&thread_handles[loop], loop,
                                 stack_test_internal_thread_popping_and_pushing_start_popping, stpps + loop);
        abstraction_thread_start(&thread_handles[loop + cpu_count], loop,
                                 stack_test_internal_thread_popping_and_pushing_start_pushing,
                                 stpps + loop + cpu_count);
    }

    for (loop = 0; loop < cpu_count * 2; loop++)
        abstraction_thread_wait(thread_handles[loop]);

    abstraction_free(thread_handles);

    for (loop = 0; loop < cpu_count * 2; loop++)
        stack_delete((stpps + loop)->local_ss, NULL, NULL);

    abstraction_free(stpps);
    vi.min_element = vi.max_element = 100000 * cpu_count * 2;
    stack_query(ss, STACK_QUERY_VALIDATE, (void *) &vi, (void *) dvs);
    stack_delete(ss, NULL, NULL);

    internal_display_test_result(2, "stack", dvs[0], "stack freelist", dvs[1]);

    return;
}

thread_return_t CALLING_CONVENTION stack_test_internal_thread_popping_and_pushing_start_popping(void *state) {
    struct stack_test_popping_and_pushing_state *stpps;
    void *data;
    time_t start_time;
    unsigned int count;

    assert(NULL != state);

    stpps = (struct stack_test_popping_and_pushing_state *) state;
    stack_use(stpps->ss);
    stack_use(stpps->local_ss);
    time(&start_time);

    while (time(NULL) < start_time + 10) {
        count = 0;
        while (count < 100000) {
            if (stack_pop(stpps->ss, &data)) {
                stack_push(stpps->local_ss, data);
                count++;
            }
        }
        while (stack_pop(stpps->local_ss, &data))
            stack_push(stpps->ss, data);
    }

    return ((thread_return_t) EXIT_SUCCESS);
}

thread_return_t CALLING_CONVENTION stack_test_internal_thread_popping_and_pushing_start_pushing(void *state) {
    struct stack_test_popping_and_pushing_state *stpps;
    void *data;
    time_t start_time;
    unsigned int count;

    assert(NULL != state);

    stpps = (struct stack_test_popping_and_pushing_state *) state;
    stack_use(stpps->ss);
    stack_use(stpps->local_ss);
    time(&start_time);

    while (time(NULL) < start_time + 10) {
        while (stack_pop(stpps->local_ss, &data))
            stack_push(stpps->ss, data);

        count = 0;

        while (count < 100000) {
            if (stack_pop(stpps->ss, &data)) {
                stack_push(stpps->local_ss, data);
                count++;
            }
        }
    }

    while (stack_pop(stpps->local_ss, &data))
        stack_push(stpps->ss, data);

    return ((thread_return_t) EXIT_SUCCESS);
}

void stack_test_internal_rapid_popping_and_pushing(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct stack_state *ss;
    struct validation_info vi;
    enum data_structure_validity dvs[2];

    internal_display_test_name("Rapid popping and pushing (10 seconds)");

    cpu_count = abstraction_cpu_count();
    stack_new(&ss, cpu_count);
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);

    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, stack_test_internal_thread_rapid_popping_and_pushing, ss);

    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);

    abstraction_free(thread_handles);
    vi.min_element = 0;
    vi.max_element = 0;
    stack_query(ss, STACK_QUERY_VALIDATE, (void *) &vi, (void *) dvs);
    stack_delete(ss, NULL, NULL);

    internal_display_test_result(2, "stack", dvs[0], "stack freelist", dvs[1]);

    return;
}

thread_return_t CALLING_CONVENTION stack_test_internal_thread_rapid_popping_and_pushing(void *state) {
    struct stack_state *ss;
    void *data = NULL;
    time_t start_time;

    assert(NULL != state);

    ss = (struct stack_state *) state;
    stack_use(ss);
    time(&start_time);

    while (time(NULL) < start_time + 10) {
        stack_push(ss, data);
        stack_pop(ss, &data);
    }

    return ((thread_return_t) EXIT_SUCCESS);
}
