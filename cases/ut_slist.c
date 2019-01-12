#include "internal.h"

void test_slist(void) {
    printf("\n"
           "Slist tests\n"
           "===========\n");
    test_slist_new_delete_get();
    test_slist_get_set();
    test_slist_delete_all_elements();
    return;
}

void test_slist_new_delete_get(void) {
    unsigned int loop, cpu_count;
    struct slist_state *ss;
    struct slist_element *se = NULL;
    struct slist_test_state *sts;
    thread_state_t *thread_handles;
    size_t total_create_count = 0,
           total_delete_count = 0,
           element_count = 0;
    enum data_structure_validity dvs = VALIDITY_VALID;
    internal_display_test_name("New head/next, delete and get next");
    cpu_count = abstraction_cpu_count();
    slist_new(&ss, NULL, NULL);
    sts = abstraction_malloc(sizeof(struct slist_test_state) * cpu_count * 2);
    for (loop = 0; loop < cpu_count * 2; loop++) {
        (sts + loop)->ss = ss;
        (sts + loop)->create_count = 0;
        (sts + loop)->delete_count = 0;
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count * 2);
    for (loop = 0; loop < cpu_count; loop++) {
        abstraction_thread_start(&thread_handles[loop], loop, slist_test_internal_thread_new_delete_get_new_head_and_next, sts + loop);
        abstraction_thread_start(&thread_handles[loop + cpu_count], loop, slist_test_internal_thread_new_delete_get_delete_and_get, sts + loop + cpu_count);
    }
    for (loop = 0; loop < cpu_count * 2; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    for (loop = 0; loop < cpu_count * 2; loop++) {
        total_create_count += (sts + loop)->create_count;
        total_delete_count += (sts + loop)->delete_count;
    }
    while (NULL != slist_pick_head(ss, &se))
        element_count++;
    if (total_create_count - total_delete_count - element_count != 0)
        dvs = VALIDITY_INVALID_TEST_DATA;
    abstraction_free(sts);
    slist_delete(ss);
    internal_display_test_result(1, "slist", dvs);
    return;
}

thread_return_t CALLING_CONVENTION slist_test_internal_thread_new_delete_get_new_head_and_next(void *state) {
    struct slist_test_state *sts;
    time_t start_time;
    struct slist_element *se = NULL;
    assert(state != NULL);
    sts = (struct slist_test_state *)state;
    slist_use(sts->ss);
    time(&start_time);
    while (time(NULL) < start_time + 1) {
        if (sts->create_count % 2 == 0)
            se = slist_new_head(sts->ss, NULL);
        else
            slist_new_next(se, NULL);
        sts->create_count++;
    }
    return ((thread_return_t)EXIT_SUCCESS);
}

thread_return_t CALLING_CONVENTION slist_test_internal_thread_new_delete_get_delete_and_get(void *state) {
    struct slist_test_state *sts;
    time_t start_time;
    struct slist_element *se = NULL;
    assert(state != NULL);
    sts = (struct slist_test_state *)state;
    slist_use(sts->ss);
    time(&start_time);
    while (time(NULL) < start_time + 1) {
        if (se == NULL)
            slist_get_head(sts->ss, &se);
        else
            slist_get_next(se, &se);
        if (se != NULL) {
            if (1 == slist_logically_delete_element(sts->ss, se))
                sts->delete_count++;
        }
    }
    return ((thread_return_t)EXIT_SUCCESS);
}

void test_slist_get_set(void) {
    unsigned int loop, cpu_count;
    struct slist_state *ss;
    struct slist_element *se = NULL;
    struct slist_test_state *sts;
    thread_state_t *thread_handles;
    atom_t thread_and_count, thread, count, *per_thread_coutners, *per_thread_drop_flags;
    enum data_structure_validity dvs = VALIDITY_VALID;
    internal_display_test_name("Get and set user data");
    cpu_count = abstraction_cpu_count();
    slist_new(&ss, NULL, NULL);
    for (loop = 0; loop < cpu_count * 10; loop++)
        slist_new_head(ss, NULL);
    sts = abstraction_malloc(sizeof(struct slist_test_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (sts + loop)->ss = ss;
        (sts + loop)->thread_and_count = (atom_t)loop << (sizeof(atom_t) * 8 - 8);
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, slist_test_internal_thread_get_set, sts + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    per_thread_coutners = abstraction_malloc(sizeof(atom_t) * cpu_count);
    per_thread_drop_flags = abstraction_malloc(sizeof(atom_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        *(per_thread_coutners + loop) = 0;
        *(per_thread_drop_flags + loop) = 0;
    }
    while (dvs == VALIDITY_VALID AND NULL != slist_pick_head(ss, &se)) {
        slist_get_data(se, (void **)&thread_and_count);
        thread = thread_and_count >> (sizeof(atom_t) * 8 - 8);
        count = (thread_and_count << 8) >> 8;
        if (thread >= cpu_count) {
            dvs = VALIDITY_INVALID_TEST_DATA;
            break;
        }
        if (per_thread_coutners[thread] == 0) {
            per_thread_coutners[thread] = count;
            continue;
        }
        per_thread_coutners[thread]++;
        if (count < per_thread_coutners[thread] AND per_thread_drop_flags[thread] == 1) {
            dvs = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
            break;
        }
        if (count < per_thread_coutners[thread] AND per_thread_drop_flags[thread] == 0) {
            per_thread_drop_flags[thread] = 1;
            per_thread_coutners[thread] = count;
            continue;
        }
        if (count < per_thread_coutners[thread])
            dvs = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
        if (count >= per_thread_coutners[thread])
            per_thread_coutners[thread] = count;
    }
    abstraction_free(per_thread_drop_flags);
    abstraction_free(per_thread_coutners);
    abstraction_free(sts);
    slist_delete(ss);
    internal_display_test_result(1, "slist", dvs);
    return;
}

thread_return_t CALLING_CONVENTION slist_test_internal_thread_get_set(void *state) {
    struct slist_test_state *sts;
    time_t start_time;
    struct slist_element *se = NULL;
    assert(NULL != state);
    sts = (struct slist_test_state *)state;
    slist_use(sts->ss);
    time(&start_time);
    while (time(NULL) < start_time + 1) {
        if (NULL == se)
            slist_get_head(sts->ss, &se);
        slist_set_data(se, (void *)sts->thread_and_count++);
        slist_get_next(se, &se);
    }
    return ((thread_return_t)EXIT_SUCCESS);
}

void test_slist_delete_all_elements(void) {
    struct slist_state *ss;
    struct slist_element *se = NULL;
    size_t element_count = 0;
    unsigned int loop;
    enum data_structure_validity dvs = VALIDITY_VALID;
    internal_display_test_name("Delete all elements");
    slist_new(&ss, NULL, NULL);
    for (loop = 0; loop < 1000000; loop++)
        slist_new_head(ss, NULL);
    slist_single_threaded_physically_delete_all_elements(ss);
    while (NULL != slist_pick_head(ss, &se))
        element_count++;
    if (element_count != 0)
        dvs = VALIDITY_INVALID_TEST_DATA;
    slist_delete(ss);
    internal_display_test_result(1, "slist", dvs);
    return;
}
