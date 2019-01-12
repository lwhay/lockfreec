#ifndef STRUCTURES_H
#define STRUCTURES_H
#include "lf_basic.h"
#include "advance/lf_hashmap.h"
#pragma pack(push, LF_ALIGN_DOUBLE_POINTER)

struct abstraction_test_cas_state {
    volatile atom_t *shared_counter;
    atom_t          local_counter;
};

struct abstraction_test_dcas_state {
    volatile atom_t *shared_counter;
    atom_t         local_counter;
};

// Freelist test
struct freelist_test_popping_state {
    struct freelist_state   *fs, *fs_thread_local;
};

struct freelist_test_pushing_state {
    atom_t          *count, thread_number;
    struct freelist_state   *source_fs, *fs;
};

struct freelist_test_popping_and_pushing_state {
    struct freelist_state   *local_fs, *fs;
};

struct freelist_test_counter_and_thread_number {
    atom_t                  thread_number;
    unsigned long long int  counter;
};

// Queue test
struct queue_test_enqueuing_state {
    struct queue_state  *qs;
    atom_t              counter;
};

struct queue_test_dequeuing_state {
    struct queue_state  *qs;
    int                 error_flag;
};

struct queue_test_enqueuing_and_dequeuing_state {
    struct queue_state  *qs;
    atom_t              counter, thread_number,*per_thread_counters;
    unsigned int        cpu_count;
    int                 error_flag;
};

struct queue_test_rapid_enqueuing_and_dequeuing_state {
    struct queue_state  *qs;
    atom_t              counter;
};

// Ringbuffer test
struct ringbuffer_test_reading_state {
    struct ringbuffer_state *rs;
    int                     error_flag;
    atom_t                  read_count;
};

struct ringbuffer_test_writing_state {
    struct ringbuffer_state *rs;
    atom_t                  write_count;
};

struct ringbuffer_test_reading_and_writing_state {
    struct ringbuffer_state *rs;
    atom_t                  counter, *per_thread_counters;
    unsigned int            cpu_count;
    int                     error_flag;
};

// Slist test
struct slist_test_state {
    struct slist_state      *ss;
    size_t                  create_count;
    size_t                  delete_count;
    atom_t                  thread_and_count;
};

// Stack test
struct stack_test_popping_state {
    struct stack_state      *ss, *ss_thread_local;
};

struct stack_test_pushing_state {
    atom_t                  thread_number;
    struct stack_state      *ss;
};

struct stack_test_popping_and_pushing_state {
    struct stack_state      *local_ss, *ss;
};

struct stack_test_counter_and_thread_number {
    atom_t                  thread_number, counter;
};

// Freelist benchmark
struct freelist_benchmark {
    struct freelist_state   *fs;
    atom_t                  operation_count;
};

// Queue benchmark
struct queue_benchmark {
    struct queue_state      *qs;
    atom_t                  operation_count;
};

// Ringbuffer benchmark
struct ringbuffer_benchmark {
    struct ringbuffer       *rs;
    atom_t                  operation_count;
};

// Stack benchmark
struct stack_benchmark {
    struct stack_state      *ss;
    atom_t                  operation_count;
};

//hashmap test state
struct hashmap_test_state{
    struct hash* hs;
    atom_t  item_count;
    atom_t  start_num;
};

#pragma pack(pop)
#endif // STRUCTURES_H
