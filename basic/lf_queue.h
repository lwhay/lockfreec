#ifndef LF_QUEUE_H
#define LF_QUEUE_H
#include "lf_basic.h"

#define LF_QUEUE_STATE_UNKNOWN              -1
#define LF_QUEUE_STATE_EMPTY                0
#define LF_QUEUE_STATE_ENQUEUE_OUT_OF_PLACE 1
#define LF_QUEUE_STATE_ATTEMPT_DELETE_QUEUE 2

#define LF_QUEUE_POINTER                    0
#define LF_QUEUE_COUNTER                    1
#define LF_QUEUE_PAC_SIZE                   2

#pragma pack(push, LF_ALIGN_DOUBLE_POINTER)
struct queue_state {
    struct queue_element
            *volatile enqueue[LF_QUEUE_PAC_SIZE],
            *volatile dequeue[LF_QUEUE_PAC_SIZE];
    atom_t aba_count;
    struct freelist_state *fs;
};

struct queue_element {
    struct queue_element *volatile next[LF_QUEUE_PAC_SIZE];
    struct freelist_element *fe;
    void* data;
};

#pragma pack(pop)

int queue_internal_freelist_init_function(void **data, void *state);
void queue_internal_freelist_delete_function(void *data, void *state);

void queue_internal_new_element_from_freelist(struct queue_state *qs, struct queue_element *qe[LF_QUEUE_PAC_SIZE], void *data);
void queue_internal_guaranteed_new_element_from_freelist(struct queue_state *qs, struct queue_element *qe[LF_QUEUE_PAC_SIZE], void *data);
void queue_internal_init_element(struct queue_state *qs, struct queue_element *qe[LF_QUEUE_PAC_SIZE], struct freelist_element *fe, void *data);

void queue_internal_queue(struct queue_state *qs, struct queue_element *qe[LF_QUEUE_PAC_SIZE]);
void queue_internal_validate(struct queue_state *qs, struct validation_info *vi, enum data_structure_validity *qv, enum data_structure_validity *fv);
#endif // LF_QUEUE_H
