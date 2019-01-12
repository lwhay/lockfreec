#include "internal.h"
#include "abstraction.h"
#define THREAD_NUM 1
#define QUEUE_NUM 100000000
void test_queue(void) {
    printf("\n"
           "Queue Test\n"
           "==========\n");
    queue_test_enqueuing();
//    queue_test_dequeuing();
//    queue_test_enqueuing_and_dequeuing();
//    queue_test_rapid_enqueuing_and_dequeuing();
}

void queue_test_enqueuing(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct queue_state *qs;
    struct queue_test_enqueuing_state *qtes;
    atom_t data, thread, count, *per_thread_counters;
    struct validation_info vi = {1000000, 1000000};
    enum data_structure_validity dvs[2];
    internal_display_test_name("Enqueuing");
    cpu_count = abstraction_cpu_count();
    cpu_count = THREAD_NUM;
    queue_new(&qs, QUEUE_NUM);
    qtes = abstraction_malloc(sizeof(struct queue_test_enqueuing_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (qtes + loop)->qs = qs;
        (qtes + loop)->counter = (atom_t)loop << (sizeof(atom_t) * 8 - 8);
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    struct timeval tpstart,tpend;
    double times;
    gettimeofday(&tpstart,NULL);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, queue_test_internal_thread_simple_enqueuer, qtes + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    gettimeofday(&tpend,NULL);
    times = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    times /= 1000000;
    printf("\n%u\t%lf\n",QUEUE_NUM,times);
    abstraction_free(thread_handles);
    abstraction_free(qtes);
    printf("simple queue over!\n");
//    queue_query(qs, QUEUE_QUERY_VALIDATE, &vi, dvs);
//    per_thread_counters = abstraction_malloc(sizeof(atom_t) * cpu_count);
//    for (loop = 0; loop < cpu_count; loop++)
//        *(per_thread_counters + loop) = 0;
//    while (dvs[0] == VALIDITY_VALID AND dvs[1] == VALIDITY_VALID AND queue_dequeue(qs, (void*)&data)) {
//        thread = data >> (sizeof(atom_t) * 8 - 8);
//        count = (data << 8) >> 8;
//        if (thread >= cpu_count) {
//            dvs[0] = VALIDITY_INVALID_TEST_DATA;
//            break;
//        }
//        if (count < per_thread_counters[thread])
//            dvs[0] = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
//        if (count > per_thread_counters[thread])
//            dvs[0] = VALIDITY_INVALID_MISSING_ELEMENTS;
//        if (count == per_thread_counters[thread])
//            per_thread_counters[thread]++;
//    }
//    abstraction_free(per_thread_counters);
//    queue_delete(qs, NULL, NULL);
//    internal_display_test_result(2, "queue", dvs[0], "queue_freelist", dvs[1]);
    return;
}

thread_return_t CALLING_CONVENTION queue_test_internal_thread_simple_enqueuer(void *state) {
    struct queue_test_enqueuing_state *qtes;
    assert(NULL != state);
    qtes = (struct queue_test_enqueuing_state*)state;
    double times = 0.0;
    struct timeval tpstart,tpend;
    gettimeofday(&tpstart,NULL);
    atom_t enqueue_num = 0;
    queue_use(qtes->qs);
    while (queue_enqueue(qtes->qs, (void*)qtes->counter++))
    {
        enqueue_num++;
    }
    gettimeofday(&tpend,NULL);
    times = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    times /= 1000000;
    printf("\n%u\t%u\t%lf\n",qtes->counter >> (sizeof(atom_t)*8-8),enqueue_num,times);
    return ((thread_return_t)EXIT_SUCCESS);
}

void queue_test_dequeuing(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct queue_state *qs;
    struct queue_test_dequeuing_state *qtds;
    struct validation_info vi = {0, 0};
    enum data_structure_validity dvs[2];
    internal_display_test_name("Dequeuing");
    cpu_count = abstraction_cpu_count();
    queue_new(&qs, 1000000);
    for (loop = 0; loop < cpu_count; loop++)
        queue_enqueue(qs, (void*)(atom_t)loop);
    qtds = abstraction_malloc(sizeof(struct queue_test_dequeuing_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (qtds + loop)->qs = qs;
        (qtds + loop)->error_flag = LOWERED;
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, queue_test_internal_thread_simple_dequeuer, qtds + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    queue_query(qs, QUEUE_QUERY_VALIDATE, (void*)&vi, (void*)dvs);
    for (loop = 0; loop < cpu_count; loop++) {
        if ((qtds + loop)->error_flag == RAISED)
            dvs[0] = VALIDITY_INVALID_TEST_DATA;
    }
    abstraction_free(qtds);
    queue_delete(qs, NULL, NULL);
    internal_display_test_result(2, "queue", dvs[0], "queue freelist", dvs[1]);
    return;
}

thread_return_t CALLING_CONVENTION queue_test_internal_thread_simple_dequeuer(void *state) {
    struct queue_test_dequeuing_state *qtds;
    atom_t *prev_data, *data;
    assert(state != NULL);
    qtds = (struct queue_test_dequeuing_state*)state;
    queue_use(qtds->qs);
    queue_dequeue(qtds->qs, (void*)&prev_data);
    while (queue_dequeue(qtds->qs, (void*)&data)) {
        if (data <= prev_data)
            qtds->error_flag = RAISED;
        prev_data = data;
    }
    return ((thread_return_t)EXIT_SUCCESS);
}

void queue_test_enqueuing_and_dequeuing(void) {
    unsigned int loop, subloop, cpu_count;
    thread_state_t *thread_handles;
    struct queue_state *qs;
    struct queue_test_enqueuing_and_dequeuing_state *qteds;
    struct validation_info vi = {0, 0};
    enum data_structure_validity dvs[2];
    internal_display_test_name("Enqueuing and dequeuing (10 seconds)");
    cpu_count = abstraction_cpu_count();
    queue_new(&qs, cpu_count);
    qteds = abstraction_malloc(sizeof(struct queue_test_enqueuing_and_dequeuing_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (qteds + loop)->qs = qs;
        (qteds + loop)->thread_number = loop;
        (qteds + loop)->counter = (atom_t)loop << (sizeof(atom_t) * 8 - 8);
        (qteds + loop)->cpu_count = cpu_count;
        (qteds + loop)->error_flag = LOWERED;
        (qteds + loop)->per_thread_counters = abstraction_malloc(sizeof(atom_t) * cpu_count);
        for (subloop = 0; subloop < cpu_count; subloop++)
            *((qteds + loop)->per_thread_counters + subloop) = 0;
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, queue_test_internal_thread_enqueuer_and_dequeuer, qteds + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    queue_query(qs, QUEUE_QUERY_VALIDATE, (void*)&vi, (void*)dvs);
    for (loop = 0; loop < cpu_count; loop++)
        if ((qteds + loop)->error_flag == RAISED)
            dvs[0] = VALIDITY_INVALID_TEST_DATA;
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_free((qteds + loop)->per_thread_counters);
    abstraction_free(qteds);
    queue_delete(qs, NULL, NULL);
    internal_display_test_result(2, "queue", dvs[0], "queue freelist", dvs[1]);
    return;
}

thread_return_t CALLING_CONVENTION queue_test_internal_thread_enqueuer_and_dequeuer(void *state) {
    struct queue_test_enqueuing_and_dequeuing_state *qteds;
    time_t start_time;
    atom_t thread, count, data;
    assert(NULL != state);
    qteds = (struct queue_test_enqueuing_and_dequeuing_state*)state;
    queue_use(qteds->qs);
    time(&start_time);
    while (time(NULL) < start_time + 10) {
        queue_enqueue(qteds->qs, (void*)(qteds->counter++));
        queue_dequeue(qteds->qs, (void*)&data);
        thread = data >> (sizeof(atom_t) * 8 - 8);
        count = (data << 8) >> 8;
        if (thread >= qteds->cpu_count)
            qteds->error_flag = RAISED;
        else {
            if (count < qteds->per_thread_counters[thread])
                qteds->error_flag = RAISED;
            if (count >= qteds->per_thread_counters[thread])
                qteds->per_thread_counters[thread] = count++;
        }
    }
    return ((thread_return_t)EXIT_SUCCESS);
}

void queue_test_rapid_enqueuing_and_dequeuing(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct queue_state *qs;
    struct queue_test_rapid_enqueuing_and_dequeuing_state *qteds;
    struct validation_info vi = {50000, 50000};
    atom_t data, thread, count, *per_thread_counters;
    enum data_structure_validity dvs[2];
    internal_display_test_name("Rapid enqueuing and Dequeuing (10 second)");
    cpu_count = abstraction_cpu_count();
    queue_new(&qs, 100000);
    for (loop = 0; loop < 50000; loop++)
        queue_enqueue(qs, NULL);
    qteds = abstraction_malloc(sizeof(struct queue_test_rapid_enqueuing_and_dequeuing_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (qteds + loop)->qs = qs;
        (qteds + loop)->counter = (atom_t)loop << (sizeof(atom_t) * 8 - 8);
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, queue_test_internal_rapid_enqueuer_and_dequeuer, qteds + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    queue_query(qs, QUEUE_QUERY_VALIDATE, (void*)&vi, (void*)dvs);
    per_thread_counters = abstraction_malloc(sizeof(atom_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        *(per_thread_counters + loop) = 0;
    while (dvs[0] == VALIDITY_VALID AND dvs[1] == VALIDITY_VALID AND queue_dequeue(qs, (void*)&data)) {
        thread = data >> (sizeof(atom_t) * 8 - 8);
        count = (data << 8) >> 8;
        if (thread >= cpu_count) {
            dvs[0] = VALIDITY_INVALID_TEST_DATA;
            break;
        }
        if (per_thread_counters[thread] == 0)
            per_thread_counters[thread] = count;
        if (count < per_thread_counters[thread])
            dvs[0] = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
        if (count >= per_thread_counters[thread])
            per_thread_counters[thread] = count + 1;
    }
    abstraction_free(per_thread_counters);
    abstraction_free(qteds);
    queue_delete(qs, NULL, NULL);
    internal_display_test_result(2, "queue", dvs[0], "queue freelist", dvs[1]);
    return;
}

thread_return_t CALLING_CONVENTION queue_test_internal_rapid_enqueuer_and_dequeuer(void *state) {
    struct queue_test_rapid_enqueuing_and_dequeuing_state *qteds;
    time_t start_time;
    atom_t data;
    assert(state != NULL);
    qteds = (struct queue_test_rapid_enqueuing_and_dequeuing_state *)state;
    queue_use(qteds->qs);
    time(&start_time);
    while (time(NULL) < start_time + 10) {
        queue_enqueue(qteds->qs, (void*)(qteds->counter++));
        queue_dequeue(qteds->qs, (void*)&data);
    }
    return ((thread_return_t)EXIT_SUCCESS);
}
