#include "lf_ringbuffer.h"

int ringbuffer_new(struct ringbuffer_state **rs, atom_t *number_elements, int (*init_func)(void **data, void *state), void *state) {
    int rv = 0;
    *rs = (struct ringbuffer_state*)aligned_malloc(sizeof(struct ringbuffer_state), LF_ALIGN_DOUBLE_POINTER);
    if (NULL != *rs) {
        freelist_new(&(*rs)->fs, number_elements, init_func, state);
        if (NULL != (*rs)->fs) {
            queue_new(&(*rs)->qs, number_elements);
            if (NULL != (*rs)->fs)
                rv = 1;
            if ((*rs)->qs == NULL) {
                aligned_free(*rs);
                *rs = NULL;
            }
        }
        if ((*rs)->fs == NULL) {
            aligned_free(*rs);
            *rs = NULL;
        }
    }
    LF_BARRIER_STORE;
    return (rv);
}

void ringbuffer_delete(struct ringbuffer_state *rs, void (*delete_func)(void *data, void *state), void *state) {
    assert(NULL != rs);
    queue_delete(rs->qs, NULL, NULL);
    freelist_delete(rs->fs, delete_func, state);
    aligned_free(rs);
    return;
}

struct freelist_element *ringbuffer_get_read(struct ringbuffer_state *rs, struct freelist_element **fe) {
    assert(NULL != rs);
    assert(NULL != fe);
    queue_dequeue(rs->qs, (void**)fe);
    return(*fe);
}

struct freelist_element *ringbuffer_get_write(struct ringbuffer_state *rs, struct freelist_element **fe, int *overwrite_flag) {
    assert(NULL != rs);
    assert(NULL != fe);
    do {
        if (NULL != overwrite_flag)
            *overwrite_flag = 0;
        freelist_pop(rs->fs, fe);
        if (NULL == *fe) {
            ringbuffer_get_read(rs, fe);
            if (NULL != overwrite_flag AND NULL != *fe)
                *overwrite_flag = 1;
        }
    } while (NULL == *fe);
    return (*fe);
}

void ringbuffer_put_read(struct ringbuffer_state *rs, struct freelist_element *fe) {
    assert(NULL != rs);
    assert(NULL != fe);
    freelist_push(rs->fs, fe);
    return;
}

void ringbuffer_put_write(struct ringbuffer_state *rs, struct freelist_element *fe) {
    assert(NULL != rs);
    assert(NULL != fe);
    queue_enqueue(rs->qs, fe);
    return;
}

#pragma warning(disable : 4100)

void ringbuffer_query(struct ringbuffer_state *rs, enum ringbuffer_query_type query_type, void *input, void *output) {
    assert(NULL != rs);
    assert(NULL != output);
    switch (query_type) {
    case RINGBUFFER_QUERY_VALIDATE:
        ringbuffer_internal_validate(rs, (struct validation_info*)input, (enum data_structure_validity*)output, ((enum data_structure_validity*)output) + 2);
        break;
    }
    return;
}

#pragma warning(default : 4100)

void ringbuffer_internal_validate(struct ringbuffer_state *rs, struct validation_info *vi, enum data_structure_validity *qv, enum data_structure_validity *fv) {
    assert(NULL != rs);
    assert(NULL != qv);
    assert(NULL != fv);
    queue_query(rs->qs, QUEUE_QUERY_VALIDATE, vi, qv);
    if (NULL != vi) {
        struct validation_info fvi;
        atom_t total_element;
        freelist_query(rs->fs, FREELIST_QUERY_ELEMENT_COUNT, NULL, (void*)&total_element);
        fvi.min_element = total_element - vi->max_element;
        fvi.max_element = total_element - vi->min_element;
        freelist_query(rs->fs, FREELIST_QUERY_VALIDATE, (void*)&fvi, (void*)fv);
    }
    if (NULL == vi)
        freelist_query(rs->fs, FREELIST_QUERY_VALIDATE, NULL, (void*)fv);
    return;
}

#pragma warning(disable : 4100)

void ringbuffer_use(struct ringbuffer_state *rs) {
    assert(NULL != rs);
    LF_BARRIER_LOAD;
    return;
}

#pragma warning(default : 4100)
