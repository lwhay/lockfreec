#include "internal.h"
#include "abstraction.h"

#define THREAD_NUM 2
#define FREELIST_NUM 50000000

void test_freelist(void) {
    printf("\n"
           "Freelist Tests\n"
           "==============\n");
    freelist_test_internal_popping();
    freelist_test_internal_pushing();
    freelist_test_internal_popping_and_pushing();
    freelist_test_internal_rapid_popping_and_pushing();
}

void freelist_test_internal_popping(void) {
    unsigned int loop, cpu_count;
    atom_t count = 0;
    thread_state_t *thread_handle;
    enum data_structure_validity dvs = VALIDITY_VALID;
    struct freelist_state *fs;
    struct freelist_element *fe;
    struct freelist_test_popping_state *ftps;
    unsigned int *found_count;

    internal_display_test_name("Popping");
    cpu_count = abstraction_cpu_count();
    cpu_count = THREAD_NUM;
//    printf("Count: %d", count);
    freelist_new(&fs, FREELIST_NUM, freelist_test_internal_popping_init, &count);
//    assert(fs != NULL);
//    printf("Count: %d", count);
//    assert(count == 0);
    ftps = abstraction_malloc(sizeof(struct freelist_test_popping_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (ftps + loop)->fs = fs;
        freelist_new(&(ftps + loop)->fs_thread_local, 0, NULL, NULL);
    }

    thread_handle = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handle[loop], loop, freelist_test_internal_thread_popping, ftps + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handle[loop]);
    abstraction_free(thread_handle);

    found_count = abstraction_malloc(sizeof(unsigned int) * FREELIST_NUM);
    for (loop = 0; loop < FREELIST_NUM; loop++)
        *(found_count + loop) = 0;
    for (loop = 0; loop < cpu_count; loop++) {
        while (freelist_pop((ftps + loop)->fs_thread_local, &fe)) {
            freelist_get_data(fe, (void **) &count);
            (*(found_count + count))++;
            freelist_push(fs, fe);
        }
    }
    for (loop = 0; loop < FREELIST_NUM AND dvs == VALIDITY_VALID; loop++) {
        if (*(found_count + loop) == 0)
            dvs = VALIDITY_INVALID_MISSING_ELEMENTS;
        if (*(found_count + loop) > 1)
            dvs = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
    }

    abstraction_free(found_count);
    for (loop = 0; loop < cpu_count; loop++)
        freelist_delete((ftps + loop)->fs_thread_local, NULL, NULL);
    abstraction_free(ftps);
    freelist_delete(fs, NULL, NULL);
    internal_display_test_result(1, "freelist", dvs);
    return;
}

#pragma wraning(disable : 4100)

int freelist_test_internal_popping_init(void **data, void *state) {
    atom_t *count;
    assert(NULL != data);
    assert(NULL != state);
    count = (atom_t *) state;
    *(atom_t *) data = (*count)++;
    return (1);
}

#pragma warning(default : 4100)

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_popping(void *state) {
    struct freelist_test_popping_state *ftps;
    struct freelist_element *fe;
    assert(NULL != state);

    ftps = (struct freelist_test_popping_state *) state;
    freelist_use(ftps->fs);
    freelist_use(ftps->fs_thread_local);

    while (freelist_pop(ftps->fs, &fe))
        freelist_push(ftps->fs_thread_local, fe);

    return ((thread_return_t) EXIT_SUCCESS);
}

void freelist_test_internal_pushing(void) {
    unsigned int loop, cpu_count;
    atom_t count = 0;
    thread_state_t *thread_handles;
    enum data_structure_validity dvs;
    struct freelist_test_pushing_state *ftps;
    struct freelist_element *fe;
    struct freelist_state *fs, *cleanup_fs;
    struct freelist_test_counter_and_thread_number *cnt, *counter_and_number_trackers;
    struct validation_info vi;

    internal_display_test_name("Pushing");
    cpu_count = abstraction_cpu_count();
    cpu_count = THREAD_NUM;

    ftps = abstraction_malloc(sizeof(struct freelist_test_pushing_state) * cpu_count);
    freelist_new(&fs, 0, NULL, NULL);
    for (loop = 0; loop < cpu_count; loop++) {
        (ftps + loop)->thread_number = (atom_t) loop;
        (ftps + loop)->count = &count;
        freelist_new(&(ftps + loop)->source_fs, FREELIST_NUM, freelist_test_internal_pushing_init,
                     (void *) (ftps + loop));
        (ftps + loop)->fs = fs;
    }

    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, freelist_test_internal_thread_pushing, ftps + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);

    freelist_new(&cleanup_fs, 0, NULL, NULL);
    counter_and_number_trackers = abstraction_malloc(
            sizeof(struct freelist_test_counter_and_thread_number) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (counter_and_number_trackers + loop)->counter = FREELIST_NUM * loop;
        (counter_and_number_trackers + loop)->thread_number = (atom_t) loop;
    }
    vi.min_element = vi.max_element = FREELIST_NUM * cpu_count;
    freelist_query(fs, FREELIST_QUERY_VALIDATE, &vi, (void *) &dvs);
    while (dvs == VALIDITY_VALID AND freelist_pop(fs, &fe)) {
        freelist_get_data(fe, (void **) &cnt);
        if (cnt->counter != (counter_and_number_trackers + cnt->thread_number)->counter++)
            dvs = VALIDITY_INVALID_MISSING_ELEMENTS;
        freelist_push(cleanup_fs, fe);
    }
    abstraction_free(counter_and_number_trackers);

    for (loop = 0; loop < cpu_count; loop++)
        freelist_delete((ftps + loop)->source_fs, NULL, NULL);
    abstraction_free(ftps);

    freelist_delete(cleanup_fs, freelist_test_internal_pushing_delete, NULL);
    freelist_delete(fs, NULL, NULL);

    internal_display_test_result(1, "freelist", dvs);
    return;
}

int freelist_test_internal_pushing_init(void **data, void *state) {
    struct freelist_test_counter_and_thread_number *ftcatn;
    struct freelist_test_pushing_state *ftps;
    assert(data != NULL);
    *data = abstraction_malloc(sizeof(struct freelist_test_counter_and_thread_number));
    ftps = (struct freelist_test_pushing_state *) state;
    ftcatn = (struct freelist_test_counter_and_thread_number *) *data;
    ftcatn->counter = (*ftps->count)++;
    ftcatn->thread_number = ftps->thread_number;
    return (1);
}

#pragma warning(disable : 4100)

void freelist_test_internal_pushing_delete(void *data, void *state) {
    assert(NULL != data);
    assert(NULL == state);
    abstraction_free(data);
    return;
}

#pragma warning(default : 4100)

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_pushing(void *state) {
    struct freelist_test_pushing_state *ftps;
    struct freelist_element *fe;
    assert(NULL != state);
    ftps = (struct freelist_test_pushing_state *) state;
    freelist_use(ftps->source_fs);
    freelist_use(ftps->fs);
    while (freelist_pop(ftps->source_fs, &fe))
        freelist_push(ftps->fs, fe);
    return ((thread_return_t) EXIT_SUCCESS);
}

void freelist_test_internal_popping_and_pushing(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    enum data_structure_validity dvs;
    struct freelist_state *fs;
    struct freelist_test_popping_and_pushing_state *pps;
    struct validation_info vi;

    internal_display_test_name("Popping and pushing (10 seconds)");
    cpu_count = abstraction_cpu_count();
    cpu_count = THREAD_NUM;
    freelist_new(&fs, FREELIST_NUM * cpu_count, NULL, NULL);
    pps = abstraction_malloc(sizeof(struct freelist_test_popping_and_pushing_state) * cpu_count * 2);
    for (loop = 0; loop < cpu_count; loop++) {
        (pps + loop)->fs = fs;
        freelist_new(&(pps + loop)->local_fs, 0, NULL, NULL);
        (pps + loop + cpu_count)->fs = fs;
        freelist_new(&(pps + loop + cpu_count)->local_fs, FREELIST_NUM, NULL, NULL);
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count * 2);

    for (loop = 0; loop < cpu_count; loop++) {
        abstraction_thread_start(&thread_handles[loop], loop,
                                 freelist_test_internal_thread_popping_and_pushing_start_popping, pps + loop);
        abstraction_thread_start(&thread_handles[loop + cpu_count], loop,
                                 freelist_test_internal_thread_popping_and_pushing_start_pushing,
                                 pps + loop + cpu_count);
    }
    for (loop = 0; loop < cpu_count * 2; loop++)
        abstraction_thread_wait(thread_handles[loop]);

    abstraction_free(thread_handles);

    for (loop = 0; loop < cpu_count * 2; loop++)
        freelist_delete((pps + loop)->local_fs, NULL, NULL);

    abstraction_free(pps);

    vi.min_element = vi.max_element = FREELIST_NUM * cpu_count * 2;
    freelist_query(fs, FREELIST_QUERY_VALIDATE, (void *) &vi, (void *) &dvs);
    freelist_delete(fs, NULL, NULL);
    internal_display_test_result(1, "freelist", dvs);
    return;
}

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_popping_and_pushing_start_popping(void *state) {
    struct freelist_test_popping_and_pushing_state *pps;
    struct freelist_element *fe;
    time_t start_time;
    unsigned int count;
    assert(state != NULL);
    pps = (struct freelist_test_popping_and_pushing_state *) state;
    freelist_use(pps->fs);
    freelist_use(pps->local_fs);
    struct timeval tpbegin, tpend;
    gettimeofday(&tpbegin, NULL);
    time(&start_time);
    while (time(NULL) < start_time + 10) {
        count = 0;
        while (count < FREELIST_NUM) {
            freelist_pop(pps->fs, &fe);
            if (fe != NULL) {
                freelist_push(pps->local_fs, fe);
                count++;
            }
        }
        while (freelist_pop(pps->local_fs, &fe))
            freelist_push(pps->fs, fe);
    }
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpbegin.tv_sec) + tpend.tv_usec - tpbegin.tv_usec;
    printf("\n%u\t%lf\n", count, timeused);
    return ((thread_return_t) EXIT_SUCCESS);
}

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_popping_and_pushing_start_pushing(void *state) {
    struct freelist_test_popping_and_pushing_state *pps;
    struct freelist_element *fe;
    time_t start_time;
    unsigned int count;
    assert(NULL != state);
    pps = (struct freelist_test_popping_and_pushing_state *) state;
    freelist_use(pps->fs);
    freelist_use(pps->local_fs);
    time(&start_time);
    while (time(NULL) < start_time + 10) {
        while (freelist_pop(pps->local_fs, &fe))
            freelist_push(pps->fs, fe);
        count = 0;
        while (count < FREELIST_NUM) {
            freelist_pop(pps->fs, &fe);
            if (fe != NULL) {
                freelist_push(pps->local_fs, fe);
                count++;
            }
        }
    }
    while (freelist_pop(pps->local_fs, &fe))
        freelist_push(pps->fs, fe);
    return ((thread_return_t) EXIT_SUCCESS);
}

void freelist_test_internal_rapid_popping_and_pushing(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct freelist_state *fs;
    struct validation_info vi;
    enum data_structure_validity dvs;

    internal_display_test_name("Rapid popping and pushing (10 seconds)");
    cpu_count = abstraction_cpu_count();
    cpu_count = THREAD_NUM;
    freelist_new(&fs, cpu_count, NULL, NULL);
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, freelist_test_internal_thread_rapid_popping_and_pushing,
                                 fs);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    vi.min_element = cpu_count;
    vi.max_element = cpu_count;
    freelist_query(fs, FREELIST_QUERY_VALIDATE, (void *) &vi, (void *) &dvs);
    freelist_delete(fs, NULL, NULL);
    internal_display_test_result(1, "freelist", dvs);
    return;
}

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_rapid_popping_and_pushing(void *state) {
    struct freelist_state *fs;
    struct freelist_element *fe;
    time_t start_time;
    assert(state != NULL);
    fs = (struct freelist_state *) state;
    freelist_use(fs);
    size_t count = 0;
    struct timeval tpbegin, tpend;
    gettimeofday(&tpbegin, NULL);
    time(&start_time);
    while (time(NULL) < start_time + 10) {
        freelist_pop(fs, &fe);
        freelist_push(fs, fe);
        count++;
    }
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpbegin.tv_sec) + tpend.tv_usec - tpbegin.tv_usec;
    printf("\n%u\t%lf\n", count, timeused);
    return ((thread_return_t) EXIT_SUCCESS);
}
