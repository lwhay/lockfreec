#ifndef INTERNAL_H
#define INTERNAL_H

#include <string.h>
#include <time.h>
#include "abstraction.h"
#include "structures.h"
#include "advance/lf_memtrie.h"
// Interfaces for abstraction

#define AND &&
#define OR  ||

#define RAISED  1
#define LOWERED 0

#define NO_FLAGS 0x0

enum test_operation {
    UNKNOWN,
    HELP,
    TEST,
    BENCHMARK
};

// Prototype interfaces
void internal_display_test_name(char *test_name);

void internal_display_test_result(unsigned int number_name_dvs_paris, ...);

void internal_display_data_structure_validity(enum data_structure_validity dvs);

// Benchmark interfaces
void benchmark_freelist(void);

thread_return_t CALLING_CONVENTION benchmark_freelist_thread_pop_and_push(void *fl_bm);

void benchmark_queue(void);

thread_return_t CALLING_CONVENTION benchmark_queue_thread_enqueue_and_dequeue(void *q_bm);

void benchmark_ringbuffer(void);

thread_return_t CALLING_CONVENTION benchmark_ringbuffer_thread_write_and_read(void *rb_bm);

void benchmark_stack(void);

thread_return_t CALLING_CONVENTION benchmark_stack_thread_push_and_pop(void *s_bm);

// Abstraction interfaces
void test_abstraction(void);

void abstraction_test_increment(void);

thread_return_t CALLING_CONVENTION abstraction_test_internal_thread_increment(void *shared_counter);

thread_return_t CALLING_CONVENTION abstraction_test_internal_thread_atomic_increment(void *shared_counter);

void abstraction_test_cas(void);

thread_return_t CALLING_CONVENTION abstraction_test_internal_thread_cas(void *cas_state);

void abstraction_test_dcas(void);

thread_return_t CALLING_CONVENTION abstraction_test_internal_thread_dcas(void *dcas_state);

// Test interfaces
// Freelist test
void test_freelist(void);

void freelist_test_internal_popping(void);

int freelist_test_internal_popping_init(void **data, void *state);

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_popping(void *state);

void freelist_test_internal_pushing(void);

int freelist_test_internal_pushing_init(void **data, void *state);

void freelist_test_internal_pushing_delete(void *data, void *state);

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_pushing(void *state);

void freelist_test_internal_popping_and_pushing(void);

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_popping_and_pushing_start_popping(void *state);

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_popping_and_pushing_start_pushing(void *state);

void freelist_test_internal_rapid_popping_and_pushing(void);

thread_return_t CALLING_CONVENTION freelist_test_internal_thread_rapid_popping_and_pushing(void *state);

// Queue test
void test_queue(void);

void queue_test_enqueuing(void);

thread_return_t CALLING_CONVENTION queue_test_internal_thread_simple_enqueuer(void *state);

void queue_test_dequeuing(void);

thread_return_t CALLING_CONVENTION queue_test_internal_thread_simple_dequeuer(void *state);

void queue_test_enqueuing_and_dequeuing(void);

thread_return_t CALLING_CONVENTION queue_test_internal_thread_enqueuer_and_dequeuer(void *state);

void queue_test_rapid_enqueuing_and_dequeuing(void);

thread_return_t CALLING_CONVENTION queue_test_internal_rapid_enqueuer_and_dequeuer(void *state);

// Ringbuffer test
void test_ringbuffer(void);

void ringbuffer_test_reading(void);

thread_return_t CALLING_CONVENTION ringbuffer_test_thread_simple_reader(void *state);

void ringbuffer_test_writing(void);

thread_return_t CALLING_CONVENTION ringbuffer_test_thread_simple_writer(void *state);

void ringbuffer_test_reading_and_writing(void);

thread_return_t CALLING_CONVENTION ringbuffer_test_thread_reader_writer(void *state);

// Slist test
void test_slist(void);

void test_slist_new_delete_get(void);

thread_return_t CALLING_CONVENTION slist_test_internal_thread_new_delete_get_new_head_and_next(void *state);

thread_return_t CALLING_CONVENTION slist_test_internal_thread_new_delete_get_delete_and_get(void *state);

void test_slist_get_set(void);

thread_return_t CALLING_CONVENTION slist_test_internal_thread_get_set(void *state);

void test_slist_delete_all_elements(void);

// Stack test
void test_stack(void);

void stack_test_internal_popping(void);

thread_return_t CALLING_CONVENTION stack_test_internal_thread_popping(void *state);

void stack_test_internal_pushing(void);

thread_return_t CALLING_CONVENTION stack_test_internal_thread_pushing(void *state);

void stack_test_internal_popping_and_pushing(void);

thread_return_t CALLING_CONVENTION stack_test_internal_thread_popping_and_pushing_start_popping(void *state);

thread_return_t CALLING_CONVENTION stack_test_internal_thread_popping_and_pushing_start_pushing(void *state);

void stack_test_internal_rapid_popping_and_pushing(void);

thread_return_t CALLING_CONVENTION stack_test_internal_thread_rapid_popping_and_pushing(void *state);

//hashmap test
void test_hashmap();

void hashmap_test_internal_put(void);

void hashmap_test_internal_get(void);

void print_bucket_node(struct hash *hs);

void hashmap_test_internal_put_and_get(void);

void hashmap_test_internal_rapid_put_and_get(void);

thread_return_t CALLING_CONVENTION hashmap_test_internal_thread_simple_insert(void *state);

thread_return_t CALLING_CONVENTION hashmap_test_internal_thread_insert_and_search(void *state);

//memtrie test
void test_memtrie();

void memtrie_test_internal_insert();

void memtrie_test_internal_search(struct trie_state *ts);

void memtrie_test_internal_insert_and_search();

thread_return_t CALLING_CONVENTION memtrie_test_internal_thread_simple_search(void *state);

void memtrie_test_internal_rapid_insert_and_search();

thread_return_t CALLING_CONVENTION memtrie_test_internal_thread_simple_insert(void *state);

thread_return_t CALLING_CONVENTION memtrie_test_internal_thread_insert_and_search(void *state);


#endif // INTERNAL_H


