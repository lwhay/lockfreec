#include "lf_queue.h"

int queue_new(struct queue_state **qs, atom_t number_elements) {
    int rv = 0;
    struct queue_element *qe[LF_QUEUE_PAC_SIZE];
    assert(qs != NULL);
    *qs = (struct queue_state*)aligned_malloc(sizeof(struct queue_state), LF_ALIGN_DOUBLE_POINTER);
    if (*qs != NULL) {
        freelist_new(&(*qs)->fs, number_elements + 1, queue_internal_freelist_init_function, NULL);
        if ((*qs)->fs != NULL) {
            queue_internal_new_element_from_freelist(*qs, qe, NULL);
            (*qs)->enqueue[LF_QUEUE_POINTER] = (*qs)->dequeue[LF_QUEUE_POINTER] = qe[LF_QUEUE_POINTER];
            (*qs)->enqueue[LF_QUEUE_COUNTER] = (*qs)->dequeue[LF_QUEUE_COUNTER] = 0;
            (*qs)->aba_count = 0;
            rv = 1;
        }
        if ((*qs)->fs == NULL) {
            printf("New error here\n");
            aligned_free(*qs);
            *qs = NULL;
        }
    }
    LF_BARRIER_STORE;
    return (rv);
}

void queue_delete(struct queue_state *qs, void (*delete_func)(void *data, void *state), void *state) {
    void *data;
    assert(NULL != qs);
    while (queue_dequeue(qs, &data))
        if (delete_func != NULL)
            delete_func(data, state);
    freelist_push(qs->fs, qs->enqueue[LF_QUEUE_POINTER]->fe);
    freelist_delete(qs->fs, queue_internal_freelist_delete_function, NULL);
    aligned_free(qs);
    return;
}

int queue_enqueue(struct queue_state *qs, void *data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct queue_element *qe[LF_QUEUE_PAC_SIZE];
    assert(NULL != qs);
    queue_internal_new_element_from_freelist(qs, qe, data);
    if (qe[LF_QUEUE_POINTER] == NULL)
        return (0);
    queue_internal_queue(qs, qe);
    return (1);
}

int queue_guaranteed_enqueue(struct queue_state *qs, void *data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER)struct queue_element *qe[LF_QUEUE_PAC_SIZE];
    assert(NULL != qs);
    queue_internal_guaranteed_new_element_from_freelist(qs, qe, data);
    if (qe[LF_QUEUE_POINTER] == NULL)
        return (0);
    queue_internal_queue(qs, qe);
    return(1);
}

void queue_internal_queue(struct queue_state *qs, struct queue_element *qe[LF_QUEUE_PAC_SIZE]) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER)struct queue_element *enqueue[LF_QUEUE_PAC_SIZE], *next[LF_QUEUE_PAC_SIZE];
    unsigned char cas_result = 0;
    assert(NULL != qs);
    assert(NULL != qe);
    LF_BARRIER_LOAD;
    do {
        enqueue[LF_QUEUE_POINTER] = qs->enqueue[LF_QUEUE_POINTER];
        enqueue[LF_QUEUE_COUNTER] = qs->enqueue[LF_QUEUE_COUNTER];
        next[LF_QUEUE_POINTER] = enqueue[LF_QUEUE_POINTER]->next[LF_QUEUE_POINTER];
        next[LF_QUEUE_COUNTER] = enqueue[LF_QUEUE_POINTER]->next[LF_QUEUE_COUNTER];
        LF_BARRIER_LOAD;
        if (qs->enqueue[LF_QUEUE_POINTER] == enqueue[LF_QUEUE_POINTER] AND qs->enqueue[LF_QUEUE_COUNTER] == enqueue[LF_QUEUE_COUNTER]) {
            if (next[LF_QUEUE_POINTER] == NULL) {
                qe[LF_QUEUE_COUNTER] = next[LF_QUEUE_COUNTER] + 1;
                cas_result = abstraction_dcas((volatile atom_t*)enqueue[LF_QUEUE_POINTER]->next, (atom_t*)qe, (atom_t*)next);
            }
            else {
                next[LF_QUEUE_COUNTER] = enqueue[LF_QUEUE_COUNTER] + 1;
                abstraction_dcas((volatile atom_t*)qs->enqueue, (atom_t*)next, (atom_t*)enqueue);
            }
        }
    } while (cas_result == 0);
    qe[LF_QUEUE_COUNTER] = enqueue[LF_QUEUE_COUNTER] + 1;
    abstraction_dcas((volatile atom_t*)qs->enqueue, (atom_t*)qe, (atom_t*)enqueue);
    return;
}

int queue_dequeue(struct queue_state *qs, void **data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER)struct queue_element *enqueue[LF_QUEUE_PAC_SIZE], *dequeue[LF_QUEUE_PAC_SIZE], *next[LF_QUEUE_PAC_SIZE];
    unsigned char cas_result = 0;
    int rv = 1, state  = LF_QUEUE_STATE_UNKNOWN, finished_flag = LOWERED;
    assert(qs != NULL);
    assert(data != NULL);
    LF_BARRIER_LOAD;
    do {
        dequeue[LF_QUEUE_POINTER] = qs->dequeue[LF_QUEUE_POINTER];
        dequeue[LF_QUEUE_COUNTER] = qs->dequeue[LF_QUEUE_COUNTER];
        enqueue[LF_QUEUE_POINTER] = qs->enqueue[LF_QUEUE_POINTER];
        enqueue[LF_QUEUE_COUNTER] = qs->enqueue[LF_QUEUE_COUNTER];
        next[LF_QUEUE_POINTER] = dequeue[LF_QUEUE_POINTER]->next[LF_QUEUE_POINTER];
        next[LF_QUEUE_COUNTER] = dequeue[LF_QUEUE_POINTER]->next[LF_QUEUE_COUNTER];
        LF_BARRIER_LOAD;
        if (dequeue[LF_QUEUE_POINTER] == qs->dequeue[LF_QUEUE_POINTER] AND dequeue[LF_QUEUE_COUNTER] == qs->dequeue[LF_QUEUE_COUNTER]) {
            if (enqueue[LF_QUEUE_POINTER] == dequeue[LF_QUEUE_POINTER] AND next[LF_QUEUE_POINTER] == NULL)
                state = LF_QUEUE_STATE_EMPTY;
            if (enqueue[LF_QUEUE_POINTER] == dequeue[LF_QUEUE_POINTER] AND next[LF_QUEUE_POINTER] != NULL)
                state = LF_QUEUE_STATE_ENQUEUE_OUT_OF_PLACE;
            if (enqueue[LF_QUEUE_POINTER] != dequeue[LF_QUEUE_POINTER])
                state = LF_QUEUE_STATE_ATTEMPT_DELETE_QUEUE;
            switch (state) {
            case LF_QUEUE_STATE_EMPTY:
                *data = NULL;
                rv = 0;
                finished_flag = RAISED;
                break;
            case LF_QUEUE_STATE_ENQUEUE_OUT_OF_PLACE:
                next[LF_QUEUE_COUNTER] = enqueue[LF_QUEUE_COUNTER] + 1;
                abstraction_dcas((volatile atom_t*)qs->enqueue, (atom_t*)next, (atom_t*)enqueue);
                break;
            case LF_QUEUE_STATE_ATTEMPT_DELETE_QUEUE:
                *data = next[LF_QUEUE_POINTER]->data;
                next[LF_QUEUE_COUNTER] = dequeue[LF_QUEUE_COUNTER] + 1;
                cas_result = abstraction_dcas((volatile atom_t*)qs->dequeue, (atom_t*)next, (atom_t*)dequeue);
                if (cas_result == 1)
                    finished_flag = RAISED;
                break;
            }
        }
    } while (finished_flag == LOWERED);
    if (cas_result == 1)
        freelist_push(qs->fs, dequeue[LF_QUEUE_POINTER]->fe);
    return(rv);
}

#pragma warning(disable : 4100)

void queue_query(struct queue_state *qs, enum queue_query_type query_type, void *input, void *output) {
    assert(NULL != qs);
    assert(NULL != output);
    switch (query_type) {
    case QUEUE_QUERY_ELEMENT_COUNT:
        assert(NULL == output);
        freelist_query(qs->fs, FREELIST_QUERY_ELEMENT_COUNT, NULL, output);
        break;
    case QUEUE_QUERY_VALIDATE:
        queue_internal_validate(qs, (struct validate_info*)input, (enum data_structure_validity*)output, ((enum data_structure_validity*)output) + 1);
        break;
    }
    return;
}

#pragma warning(default : 4100)

void queue_internal_validate(struct queue_state *qs, struct validation_info *vi, enum data_structure_validity *qv, enum data_structure_validity *fv) {
    struct queue_element *qe, *qe_slow, *qe_fast;
    atom_t element_count = 0, total_elements;
    struct validation_info fvi;
    assert(NULL != qs);
    assert(NULL != qv);
    assert(NULL != fv);
    *qv = VALIDITY_VALID;
    LF_BARRIER_LOAD;
    qe_slow = qe_fast = (struct queue_element *)qs->dequeue[LF_QUEUE_POINTER];
    if (qe_slow != NULL) {
        do {
            qe_slow = qe_slow->next[LF_QUEUE_POINTER];
            if (qe_fast != NULL)
                qe_fast = qe_fast->next[LF_QUEUE_POINTER];
            if (qe_fast != NULL)
                qe_fast = qe_fast->next[LF_QUEUE_POINTER];
        } while (qe_slow != NULL AND qe_fast != qe_slow);
    }
    if (qe_fast != NULL AND qe_slow != NULL AND qe_fast == qe_slow)
        *qv = VALIDITY_INVALID_LOOP;
    if (*qv == VALIDITY_VALID AND vi != NULL) {
        qe = (struct queue_element*)qs->dequeue[LF_QUEUE_POINTER];
        while (qe != NULL) {
            element_count++;
            qe = (struct queue_element*)qe->next[LF_QUEUE_POINTER];
        }
        element_count--;
        if (element_count < vi->min_element)
            *qv = VALIDITY_INVALID_MISSING_ELEMENTS;
        if (element_count > vi->max_element)
            *qv = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
    }
    if (vi != NULL) {
        freelist_query(qs->fs, FREELIST_QUERY_ELEMENT_COUNT, NULL, (void*)&total_elements);
        total_elements--;
        fvi.min_element = total_elements - vi->max_element;
        fvi.max_element = total_elements - vi->min_element;
        freelist_query(qs->fs, FREELIST_QUERY_VALIDATE, (void*)&fvi, (void*)fv);
    }
    if (vi == NULL)
        freelist_query(qs->fs, FREELIST_QUERY_VALIDATE, NULL, (void*)fv);
    return;
}

#pragma warning(disable : 4100)

void queue_internal_freelist_delete_function(void *data, void *state) {
    assert(data != NULL);
    assert(state == NULL);
    aligned_free(data);
    return;
}

//#pragma warning(default : 4100)

#pragma warning(disable : 4100)

void queue_use(struct queue_state *qs) {
    assert (NULL != qs);
    LF_BARRIER_LOAD;
    return;
}

//#pragma warning(default : 4100)

#pragma warning(disabe : 4100)

int queue_internal_freelist_init_function(void **data, void *state) {
    int rv = 0;
    assert(NULL != data);
    assert(NULL == state);
    *data = aligned_malloc(sizeof(struct queue_element), LF_ALIGN_DOUBLE_POINTER);
    if (NULL != *data)
        rv = 1;
    return(rv);
}

#pragma warning(default : 4100)

void queue_internal_new_element_from_freelist(struct queue_state *qs, struct queue_element *qe[LF_QUEUE_PAC_SIZE], void *data) {
    struct freelist_element *fe;
    assert(qs != NULL);
    assert(qe != NULL);
    qe[LF_QUEUE_POINTER] = NULL;
    freelist_pop(qs->fs, &fe);
    if (fe != NULL)
        queue_internal_init_element(qs, qe, fe, data);
    return;
}

void queue_internal_guaranteed_new_element_from_freelist(struct queue_state *qs, struct queue_element *qe[LF_QUEUE_PAC_SIZE], void *data) {
    struct freelist_element *fe;
    assert(qs != NULL);
    assert(qe != NULL);
    qe[LF_QUEUE_POINTER] = NULL;
    freelist_guaranteed_pop(qs->fs, &fe);
    if (fe != NULL)
        queue_internal_init_element(qs, qe, fe, data);
    return;
}

void queue_internal_init_element(struct queue_state *qs, struct queue_element *qe[LF_QUEUE_PAC_SIZE], struct freelist_element *fe, void *data) {
    assert(qs != NULL);
    assert(qe != NULL);
    assert(fe != NULL);
    freelist_get_data(fe, (void**)&qe[LF_QUEUE_POINTER]);
    qe[LF_QUEUE_COUNTER] = (struct queue_element*)abstraction_increment((atom_t*)&qs->aba_count);
    qe[LF_QUEUE_POINTER]->next[LF_QUEUE_POINTER] = NULL;
    qe[LF_QUEUE_POINTER]->next[LF_QUEUE_COUNTER] = (struct queue_element*)abstraction_increment((atom_t*)&qs->aba_count);
    qe[LF_QUEUE_POINTER]->fe = fe;
    qe[LF_QUEUE_POINTER]->data = data;
    return;
}
