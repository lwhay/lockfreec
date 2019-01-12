#include "internal.h"
#include "abstraction.h"
#include "advance/lf_hashmap.h"
#include <time.h>

#define CPU_COUNT_NUM 4
#define HASH_BUCKET   23

void test_hashmap(void) {
    printf("\n"
           "HashMap Tests\n"
           "==============\n");
    hashmap_test_internal_put();
    //hashmap_test_internal_get();
    //hashmap_test_internal_put_and_get();
    //hashmap_test_internal_rapid_put_and_get();
}

void hashmap_test_internal_put(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct hash *hs;
    struct hashmap_test_state *hts;
    atom_t data, thread, count, *per_thread_counters;
    struct validation_info vi = {1000000, 1000000};
    enum data_structure_validity dvs[2];
    internal_display_test_name("Hashmap insert...");
    cpu_count = abstraction_cpu_count();
    cpu_count = CPU_COUNT_NUM;
    if (!(hash_new(&hs, HASH_BUCKET))) {
        internal_display_test_name("hashmap new over...\n");
        return;
    }
    internal_display_test_name("hashmap test  2...");
    hts = abstraction_malloc(sizeof(struct hashmap_test_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (hts + loop)->hs = hs;
        (hts + loop)->item_count = (atom_t) loop << (sizeof(atom_t) * 8 - 8);
        (hts + loop)->start_num = MAX_HASH_DATA_NUM / cpu_count * 2;
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, hashmap_test_internal_thread_simple_insert, hts + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    //print_bucket_node(hs);
    abstraction_free(thread_handles);
    abstraction_free(hts);
    //test
    // Exists error when free hashmap.
    hash_free(hs);
    internal_display_test_name("hashmap_simple_insert over !\n");
    return;
}

void print_bucket_node(struct hash *hs) {
    atom_t loop;
    struct hash_item *node;
    int node_num_list[5] = {0};
    for (loop = 0; loop < hs->bucket_max; ++loop) {
        node = hs->table[loop]->next;
        atom_t node_num = 0;
        while (node != NULL) {
            node_num++;
            node = node->next;
        }
        if (node_num < 6)
            node_num_list[0]++;
        else if (node_num < 11)
            node_num_list[1]++;
        else if (node_num < 16)
            node_num_list[2]++;
        else if (node_num < 21)
            node_num_list[3]++;
        else
            node_num_list[4]++;
    }
    printf("\n%d\t%d\t%d\t%d\t%d\n", node_num_list[0], node_num_list[1], node_num_list[2], node_num_list[3],
           node_num_list[4]);
}

thread_return_t CALLING_CONVENTION hashmap_test_internal_thread_simple_insert(void *state) {
    struct hashmap_test_state *hts;
    struct hash_data *data;
    atom_t add_num = 0, replace_num = 0, duplication = 0, deal_num = 0, key_start;
    clock_t start_time, end_time;
    assert(NULL != state);
    hts = (struct hashmap_test_state *) state;
    hashmap_use(hts->hs);
    key_start = hts->start_num * (hts->item_count >> (sizeof(atom_t) * 8 - 8));
    int i;
    atom_t retry = 0, retry_element = 0, add_retry = 0;
    struct timeval tpstart, tpend;
    srand((unsigned) time(NULL));
    //For testing purposes.
    int k;
    int size = MAX_HASH_DATA_NUM;
    int *tmpd = (int *) malloc(sizeof(int) * (size));
    for (k = 0; k < size; k++) {
        tmpd[i] = rand() * rand() + rand();
    }
    k = 0;
    gettimeofday(&tpstart, NULL);
    while (lf_hash_internal_new_data_from_datalist(hts->hs, &data,
                                                   &retry) /*&& deal_num < MAX_HASH_DATA_NUM/CPU_COUNT_NUM*/) {
        //data->key = key_start;
        data->key = tmpd[k];
        data->val = k++;
        switch (lf_hash_add(hts->hs, data, &retry_element, &add_retry)) {
            case HASH_MAP_NODE_ADD:
                add_num++;
                break;
            case HASH_MAP_NODE_REPLACE:
                replace_num++;
                break;
            case HASH_MAP_NODE_EXIST:
                duplication++;
            default:
                break;
        }
//        key_start++;
        deal_num++;
    }
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    timeused /= 1000000;
    printf("\n%u\t%u\t%lf\t%u\t%u\t%u\t%u\t%u\t%u\n", hts->item_count >> (sizeof(atom_t) * 8 - 8), deal_num, timeused,
           add_num, replace_num, duplication, retry, retry_element, add_retry);
    free(tmpd);
    return ((thread_return_t) EXIT_SUCCESS);
}

void hashmap_test_internal_get(void) {

}

void hashmap_test_internal_put_and_get(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct hash *hs;
    struct hashmap_test_state *hts;
    atom_t data, thread, count, *per_thread_counters;
    struct validation_info vi = {1000000, 1000000};
    enum data_structure_validity dvs[2];
    internal_display_test_name("Hashmap insert...");
    cpu_count = abstraction_cpu_count();

    if (!(hash_new(&hs, 16))) {
        internal_display_test_name("hashmap new over...");
        return;
    }
    internal_display_test_name("hashmap test  2...");
    hts = abstraction_malloc(sizeof(struct hashmap_test_state) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (hts + loop)->hs = hs;
        (hts + loop)->item_count = (atom_t) loop << (sizeof(atom_t) * 8 - 8);
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, hashmap_test_internal_thread_insert_and_search,
                                 hts + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    abstraction_free(hts);
    //test
    hash_free(hs);
    internal_display_test_name("hashmap_simple_insert over !");
    return;
}

thread_return_t CALLING_CONVENTION hashmap_test_internal_thread_insert_and_search(void *state) {
    struct hashmap_test_state *hts;
    struct hash_data *data;
    atom_t add_num = 0, replace_num = 0, duplication = 0, search_succ = 0, search_fail = 0;
    clock_t start_time, end_time;
    assert(NULL != state);
    hts = (struct hashmap_test_state *) state;
    hashmap_use(hts->hs);
    int i;
    atom_t retry = 0, retry_element = 0, add_retry = 0;
    struct timeval tpstart, tpend;
    gettimeofday(&tpstart, NULL);
    while (lf_hash_internal_new_data_from_datalist(hts->hs, &data, &retry)) {
        switch (lf_hash_add(hts->hs, data, &retry_element, &add_retry)) {
            case HASH_MAP_NODE_ADD:
                add_num++;
                break;
            case HASH_MAP_NODE_REPLACE:
                replace_num++;
                break;
            case HASH_MAP_NODE_EXIST:
                duplication++;
            default:
                break;
        }

        //then to search;
        if (lf_hash_search(hts->hs, data))
            search_succ++;
        else
            search_fail++;
    }
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    timeused /= 1000000;
    printf("\npthread_t:%u", pthread_self());
    printf("\nexecute time : %lf seconds\n", timeused);
    printf("add_num:%u   replace_num:%u   duplication:%u\n", add_num, replace_num, duplication);
    printf("search_succ_num:%u    search_fail_num:%u\n\n", search_succ, search_fail);
    return ((thread_return_t) EXIT_SUCCESS);
}


void hashmap_test_internal_rapid_put_and_get(void) {

}
