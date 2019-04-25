#ifndef LF_BASIC_H
#define LF_BASIC_H

#ifdef __cplusplus
extern "C" {
#endif

#if (defined __GNUC__ && defined __x86_64__)
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
typedef unsigned long long int atom_t;
#define LF_INLINE                   inline
#define LF_ALIGN(alignment)         __attribute__( (aligned(alignment)) )
#define LF_ALIGN_SINGLE_POINTER     8
#define LF_ALIGN_DOUBLE_POINTER     16
#define LF_BARRIER_COMPILER_LOAD    __asm__ __volatile__ ( "" : : : "memory" )
#define LF_BARRIER_COMPILER_STORE   __asm__ __volatile__ ( "" : : : "memory" )
#define LF_BARRIER_COMPILER_FULL    __asm__ __volatile__ ( "" : : : "memory" )
#define LF_BARRIER_PROCESSOR_LOAD   __sync_synchronize()
#define LF_BARRIER_PROCESSOR_STORE  __sync_synchronize()
#define LF_BARRIER_PROCESSOR_FULL   __sync_synchronize()
#else
#include <assert.h>
#endif

#define LF_BARRIER_LOAD     LF_BARRIER_COMPILER_LOAD;   LF_BARRIER_PROCESSOR_LOAD;  LF_BARRIER_COMPILER_LOAD
#define LF_BARRIER_STORE    LF_BARRIER_COMPILER_STORE;  LF_BARRIER_PROCESSOR_STORE; LF_BARRIER_COMPILER_STORE
#define LF_BARRIER_FULL     LF_BARRIER_COMPILER_FULL;   LF_BARRIER_PROCESSOR_FULL;  LF_BARRIER_COMPILER_FULL

#define AND &&
#define OR  ||

#define RAISED  1
#define LOWERED 0

#define NO_FLAGS 0x0

void *aligned_malloc(size_t size, size_t align_in_bytes);
void aligned_free(void *memory);

/*static  LF_INLINE */atom_t abstraction_cas(volatile atom_t *destination, atom_t exchange, atom_t compare);
/*static  LF_INLINE */unsigned char abstraction_dcas(volatile atom_t *destination, atom_t *exchange, atom_t *compare);
/*static  LF_INLINE */atom_t abstraction_increment(volatile atom_t *value);
/*static  LF_INLINE */atom_t abstraction_sub(volatile atom_t *value);
// Structures for validity

enum data_structure_validity {
    VALIDITY_VALID,
    VALIDITY_INVALID_LOOP,
    VALIDITY_INVALID_MISSING_ELEMENTS,
    VALIDITY_INVALID_ADDITIONAL_ELEMENTS,
    VALIDITY_INVALID_TEST_DATA
};

struct validation_info {
    atom_t min_element, max_element;
};

void *abstraction_malloc(size_t size);
void abstraction_free(void *memory);

// Interfaces for freelist

enum freelist_query_type {
    FREELIST_QUERY_ELEMENT_COUNT,
    FREELIST_QUERY_VALIDATE
};

struct freelist_state;
struct freelist_element;

int freelist_new(struct freelist_state **fs, atom_t number_elements, int (*init_func)(void **data, void *state),
                 void *state);
void freelist_use(struct freelist_state *fs);
void freelist_delete(struct freelist_state *fs, void (*delete_func)(void *data, void *state), void *state);

atom_t freelist_new_elements(struct freelist_state *fs, atom_t number_elements);

struct freelist_element *freelist_pop(struct freelist_state *fs, struct freelist_element **fe);
struct freelist_element *freelist_guaranteed_pop(struct freelist_state *fs, struct freelist_element **fe);
void freelist_push(struct freelist_state *fs, struct freelist_element *fe);

void *freelist_get_data(struct freelist_element *fe, void **data);
void freelist_set_data(struct freelist_element *fe, void *data);

void freelist_query(struct freelist_state *fs, enum freelist_query_type query_type, void *input, void *output);

// Interfaces for test helper

void abstraction_test_helper_increment_non_atomic(atom_t *shared_counter);
void abstraction_test_helper_increment_atomic(atom_t *shared_counter);
void abstraction_test_helper_cas(volatile atom_t *shared_counter, atom_t *local_counter);
void abstraction_test_helper_dcas(volatile atom_t *shared_counter, atom_t *local_counter);

// Interfaces for queue
enum queue_query_type {
    QUEUE_QUERY_ELEMENT_COUNT,
    QUEUE_QUERY_VALIDATE
};

struct queue_state;

int queue_new(struct queue_state **sq, atom_t number_elements);
void queue_use(struct queue_state *qs);
void queue_delete(struct queue_state *qs, void (*delete_func)(void *data, void *state), void *state);

int queue_enqueue(struct queue_state *qs, void *data);
int queue_guaranteed_enqueue(struct queue_state *qs, void *data);
int queue_dequeue(struct queue_state *qs, void **data);

void queue_query(struct queue_state *qs, enum queue_query_type query_type, void *input, void *output);

// Interfaces for ringbuffer

enum ringbuffer_query_type {
    RINGBUFFER_QUERY_VALIDATE
};

struct ringbuffer_state;

int ringbuffer_new(struct ringbuffer_state **rs, atom_t *number_elements, int (*init_func)(void **data, void *state),
                   void *state);
void ringbuffer_use(struct ringbuffer_state *rs);
void ringbuffer_delete(struct ringbuffer_state *rs, void (*delete_func)(void *data, void *state), void *state);

struct freelist_element *ringbuffer_get_read(struct ringbuffer_state *rs, struct freelist_element **fe);
struct freelist_element *
ringbuffer_get_write(struct ringbuffer_state *rs, struct freelist_element **fe, int *overwrite_flag);

void ringbuffer_put_read(struct ringbuffer_state *rs, struct freelist_element *fe);
void ringbuffer_put_write(struct ringbuffer_state *rs, struct freelist_element *fe);

void ringbuffer_query(struct ringbuffer_state *rs, enum ringbuffer_query_type query_type, void *input, void *output);

// Interfaces for slist

struct slist_state;
struct slist_element;

int slist_new(struct slist_state **ss, void (*delete_func)(void *data, void *state), void *state);
void slist_use(struct slist_state *ss);
void slist_delete(struct slist_state *ss);

struct slist_element *slist_new_head(struct slist_state *ss, void *data);
struct slist_element *slist_new_next(struct slist_element *se, void *data);

int slist_logically_delete_element(struct slist_state *ss, struct slist_element *se);
void slist_single_threaded_physically_delete_all_elements(struct slist_state *ss);

int slist_get_data(struct slist_element *se, void **data);
int slist_set_data(struct slist_element *se, void *data);

struct slist_element *slist_get_head(struct slist_state *ss, struct slist_element **se);
struct slist_element *slist_get_next(struct slist_element *se, struct slist_element **nse);
struct slist_element *slist_pick_head(struct slist_state *ss, struct slist_element **se);

// Interfaces for stack

enum stack_query_type {
    STACK_QUERY_ELEMENT_COUNT,
    STACK_QUERY_VALIDATE
};

struct stack_state;

int stack_new(struct stack_state **ss, atom_t number_elements);
void stack_use(struct stack_state *ss);
void stack_delete(struct stack_state *ss, void (*delete_func)(void *data, void *state), void *state);

void stack_clear(struct stack_state *ss, void (*clear_func)(void *data, void *state), void *state);

int stack_push(struct stack_state *ss, void *data);
int stack_guaranteed_push(struct stack_state *ss, void *data);
int stack_pop(struct stack_state *ss, void **data);

void stack_query(struct stack_state *ss, enum stack_query_type query_type, void *input, void *output);

#ifdef __cplusplus
}
#endif

#endif // LF_BASIC_H
