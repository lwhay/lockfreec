#include "internal.h"
#include "abstraction.h"

void test_ringbuffer(void) {
    printf("\n"
           "Ringbuffer Tests\n"
           "================\n");
    ringbuffer_test_reading();
    ringbuffer_test_writing();
    ringbuffer_test_reading_and_writing();
}

void ringbuffer_test_reading(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct ringbuffer_state *rs;
    struct freelist_element *fe;
    struct ringbuffer_test_reading_state *rtrs;
    struct validation_info vi = {0, 0};
    enum data_structure_validity dvs[3];
    atom_t total_read = 0;
    internal_display_test_name("Reading");
    cpu_count = abstraction_cpu_count();
    ringbuffer_new(&rs, 1000000, NULL, NULL);
    for (loop = 0; loop < 1000000; loop++) {
        ringbuffer_get_write(rs, &fe, NULL);
        freelist_set_data(fe, (void*)(atom_t)loop);
        ringbuffer_put_write(rs, fe);
    }
    rtrs = abstraction_malloc(sizeof(struct ringbuffer_test_reading_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (rtrs + loop)->rs = rs;
        (rtrs + loop)->read_count = 0;
        (rtrs + loop)->error_flag = LOWERED;
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, ringbuffer_test_thread_simple_reader, rtrs + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    ringbuffer_query(rs, RINGBUFFER_QUERY_VALIDATE, (void*)&vi, (void*)dvs);
    for (loop = 0; loop < cpu_count; loop++)
        if ((rtrs + loop)->error_flag == RAISED)
            dvs[0] = VALIDITY_INVALID_TEST_DATA;
    for (loop = 0; loop < cpu_count; loop++)
        total_read += (rtrs + loop)->read_count;
    if (total_read < 1000000)
        dvs[0] = VALIDITY_INVALID_MISSING_ELEMENTS;
    if (total_read > 1000000)
        dvs[0] = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
    abstraction_free(rtrs);
    ringbuffer_delete(rs, NULL, NULL);
    internal_display_test_result(3, "queue", dvs[0], "queue freelist", dvs[1], "freelist", dvs[2]);
    return;
}

thread_return_t CALLING_CONVENTION ringbuffer_test_thread_simple_reader(void *state) {
    struct ringbuffer_test_reading_state *rtrs;
    struct freelist_element *fe;
    atom_t *prev_data, *data;
    assert(NULL != state);
    rtrs = (struct ringbuffer_test_reading_state *)state;
    ringbuffer_use(rtrs->rs);
    ringbuffer_get_read(rtrs->rs, &fe);
    if (NULL == fe)
        return ((thread_return_t)EXIT_SUCCESS);
    freelist_get_data(fe, (void**)&prev_data);
    ringbuffer_put_read(rtrs->rs, fe);
    rtrs->read_count++;
    while (ringbuffer_get_read(rtrs->rs, &fe)) {
        freelist_get_data(fe, (void**)&data);
        ringbuffer_put_read(rtrs->rs, fe);
        if (data <= prev_data)
            rtrs->error_flag = RAISED;
        prev_data = data;
        rtrs->read_count++;
    }
    return ((thread_return_t)EXIT_SUCCESS);
}

void ringbuffer_test_writing(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct ringbuffer_state *rs;
    struct freelist_element *fe;
    struct ringbuffer_test_writing_state *rtws;
    struct validation_info vi = {100000, 100000};
    enum data_structure_validity dvs[3];
    atom_t thread, count, data, *per_thread_counters;
    internal_display_test_name("Writing (10 secondes)");
    cpu_count = abstraction_cpu_count();
    ringbuffer_new(&rs, 100000, NULL, NULL);
    rtws = abstraction_malloc(sizeof(struct ringbuffer_test_writing_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (rtws + loop)->rs = rs;
        (rtws + loop)->write_count = (atom_t)loop << (sizeof(atom_t) * 8 - 8);
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t*) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, ringbuffer_test_thread_simple_writer, rtws + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    per_thread_counters = abstraction_malloc(sizeof(atom_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        *(per_thread_counters + loop) = 0;
    ringbuffer_query(rs, RINGBUFFER_QUERY_VALIDATE, (void*)&vi, (void*)dvs);
    while (dvs[0] == VALIDITY_VALID AND dvs[1] == VALIDITY_VALID AND dvs[2] == VALIDITY_VALID AND ringbuffer_get_read(rs, &fe)) {
        freelist_get_data(fe, (void*)&data);
        thread = data >> (sizeof(atom_t) * 8 - 8);
        count = (data << 8) >> 8;
        if (thread >= cpu_count) {
            dvs[0] = VALIDITY_INVALID_TEST_DATA;
            ringbuffer_put_read(rs, fe);
            break;
        }
        if (per_thread_counters[thread] == 0)
            per_thread_counters[thread] = count;
        if (count < per_thread_counters[thread])
            dvs[0] = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
        if (count >= per_thread_counters[thread])
            per_thread_counters[thread] = count++;
        ringbuffer_put_read(rs, fe);
    }
    abstraction_free(per_thread_counters);
    abstraction_free(rtws);
    ringbuffer_delete(rs, NULL, NULL);
    internal_display_test_result(3, "queue", dvs[0], "queue freelist", dvs[1], "freelist", dvs[2]);
    return;
}

thread_return_t CALLING_CONVENTION ringbuffer_test_thread_simple_writer(void *state) {
    struct ringbuffer_test_writing_state *rtws;
    struct freelist_element *fe;
    time_t start_time;
    assert(NULL != state);
    rtws = (struct ringbuffer_test_writing_state*)state;
    ringbuffer_use(rtws->rs);
    time(&start_time);
    while (time(NULL) < start_time + 10) {
        ringbuffer_get_write(rtws->rs, &fe, NULL);
        freelist_set_data(fe, (void*)(atom_t)(rtws->write_count++));
        ringbuffer_put_write(rtws->rs, fe);
    }
    return ((thread_return_t)EXIT_SUCCESS);
}

void ringbuffer_test_reading_and_writing(void) {
    unsigned int loop, subloop, cpu_count;
    thread_state_t *thread_handles;
    struct ringbuffer_state *rs;
    struct ringbuffer_test_reading_and_writing_state *rtrws;
    struct validation_info vi = {0, 0};
    enum data_structure_validity dvs[3];
    internal_display_test_name("Reading and writing (10 seconds)");
    cpu_count = abstraction_cpu_count();
    ringbuffer_new(&rs, 100000, NULL, NULL);
    rtrws = abstraction_malloc(sizeof(struct ringbuffer_test_reading_and_writing_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (rtrws + loop)->rs = rs;
        (rtrws + loop)->counter = (atom_t)loop << (sizeof(atom_t) * 8 - 8);
        (rtrws + loop)->cpu_count = cpu_count;
        (rtrws + loop)->error_flag = LOWERED;
        (rtrws + loop)->per_thread_counters = abstraction_malloc(sizeof(atom_t) * cpu_count);
        for (subloop = 0; subloop < cpu_count; subloop++)
            *((rtrws + loop)->per_thread_counters + subloop) = 0;
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, ringbuffer_test_thread_reader_writer, rtrws + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    ringbuffer_query(rs, RINGBUFFER_QUERY_VALIDATE, (void*)&vi, (void*)dvs);
    for (loop = 0; loop < cpu_count; loop++)
        if ((rtrws + loop)->error_flag == RAISED)
            dvs[0] = VALIDITY_INVALID_TEST_DATA;
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_free((rtrws + loop)->per_thread_counters);
    abstraction_free(rtrws);
    ringbuffer_delete(rs, NULL, NULL);
    internal_display_test_result(3, "queue", dvs[0], "queue freelist", dvs[1], "freelist", dvs[2]);
    return;
}

thread_return_t CALLING_CONVENTION ringbuffer_test_thread_reader_writer(void *state) {
    struct ringbuffer_test_reading_and_writing_state *rtrws;
    struct freelist_element *fe;
    atom_t data, thread, count;
    time_t start_time;
    assert(NULL != state);
    rtrws = (struct ringbuffer_test_reading_and_writing_state *)state;
    time(&start_time);
    while (time(NULL) < start_time + 10) {
        ringbuffer_get_write(rtrws->rs, &fe, NULL);
        freelist_set_data(fe, (void*)(atom_t)(rtrws->counter++));
        ringbuffer_put_write(rtrws->rs, fe);
        ringbuffer_get_read(rtrws->rs, &fe);
        freelist_get_data(fe, (void*)&data);
        thread = data >> (sizeof(atom_t) * 8 - 8);
        count = (data << 8) >> 8;
        if (thread >= rtrws->cpu_count)
            rtrws->error_flag = RAISED;
        else {
            if (count < rtrws->per_thread_counters[thread])
                rtrws->error_flag = RAISED;
            if (count >= rtrws->per_thread_counters[thread])
                rtrws->per_thread_counters[thread] = count + 1;
        }
        ringbuffer_put_read(rtrws->rs, fe);
    }
    return((thread_return_t)EXIT_SUCCESS);
}
