#include "lf_freelist.h"

atom_t freelist_internal_new_element(struct freelist_state *fs, struct freelist_element **fe) {
    atom_t rv = 0;
    assert(fs != NULL);
    assert(fe != NULL);

    *fe = (struct freelist_element *) aligned_malloc(sizeof(struct freelist_element), LF_ALIGN_DOUBLE_POINTER);
    if (NULL != fe) {
        if (fs->init_func == NULL) {
            (*fe)->data = NULL;
            rv = 1;
        }

        if (fs->init_func != NULL) {
            rv = fs->init_func(&(*fe)->data, fs->state);

            if (rv == 0) {
                aligned_free(*fe);
                *fe = NULL;
            }
        }
    }
    if (rv == 1)
        abstraction_increment((atom_t *) &fs->element_count);

    return (rv);
}

atom_t freelist_new_elements(struct freelist_state *fs, atom_t number_elements) {
    struct freelist_element *fe;
    atom_t loop, count = 0;
    assert(fs != NULL);

    for (loop = 0; loop < number_elements; loop++)
        if (freelist_internal_new_element(fs, &fe)) {
            freelist_push(fs, fe);
            count++;
        }

    return (count);
}

#pragma warning(disable : 4100)

void freelist_use(struct freelist_state *fs) {
    assert(fs != NULL);
    LF_BARRIER_LOAD;
    return;
}

#pragma wraning(default : 4100)

int freelist_new(struct freelist_state **fs, atom_t number_elements, int (*init_func)(void **data, void *state),
                 void *state) {
    int rv = 0;
    atom_t element_count;
    *fs = (struct freelist_state *) aligned_malloc(sizeof(struct freelist_state), LF_ALIGN_DOUBLE_POINTER);

    if ((*fs) != NULL) {
        (*fs)->top[LF_FREELIST_POINTER] = NULL;
        (*fs)->top[LF_FREELIST_COUNTER] = 0;
        (*fs)->init_func = init_func;
        (*fs)->state = state;
        (*fs)->aba_counter = 0;
        (*fs)->element_count = 0;

        element_count = freelist_new_elements(*fs, number_elements);

        if (element_count == number_elements)
            rv = 1;

        if (element_count != number_elements) {
            aligned_free(*fs);
            *fs = NULL;
        }
    }
    LF_BARRIER_STORE;
    return (rv);
}

void freelist_delete(struct freelist_state *fs, void (*delete_func)(void *data, void *state), void *state) {
    struct freelist_element *fe;
    void *data;
    assert(fs != NULL);

    while (freelist_pop(fs, &fe)) {
        if (delete_func != NULL) {
            freelist_get_data(fe, &data);
            delete_func(data, state);
        }
        aligned_free(fe);
    }

    aligned_free(fs);

    return;
}

struct freelist_element *freelist_pop(struct freelist_state *fs, struct freelist_element **fe) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct freelist_element *fe_local[LF_FREELIST_PAC_SIZE];

    assert(NULL != fs);
    assert(NULL != fe);

    LF_BARRIER_LOAD;
    fe_local[LF_FREELIST_COUNTER] = fs->top[LF_FREELIST_COUNTER];
    fe_local[LF_FREELIST_POINTER] = fs->top[LF_FREELIST_POINTER];

    do {
        if (fe_local[LF_FREELIST_POINTER] == NULL) {
            *fe = NULL;
            return (*fe);
        }
    } while (0 == abstraction_dcas((volatile atom_t *) fs->top, (atom_t *) fe_local[LF_FREELIST_POINTER]->next,
                                   (atom_t *) fe_local));

    *fe = (struct freelist_element *) fe_local[LF_FREELIST_POINTER];
    return (*fe);
}

struct freelist_element *freelist_guaranteed_pop(struct freelist_state *fs, struct freelist_element **fe) {
    assert(NULL != fs);
    assert(NULL != fe);
    freelist_internal_new_element(fs, fe);
    return (*fe);
}

void freelist_push(struct freelist_state *fs, struct freelist_element *fe) {
    LF_ALIGN(
            LF_ALIGN_DOUBLE_POINTER) struct freelist_element *fe_local[LF_FREELIST_PAC_SIZE], *original_fe_next[LF_FREELIST_PAC_SIZE];
    assert(NULL != fs);
    assert(NULL != fe);
    LF_BARRIER_LOAD;
    fe_local[LF_FREELIST_POINTER] = fe;
    fe_local[LF_FREELIST_COUNTER] = (struct freelist_element *) abstraction_increment((atom_t *) &fs->aba_counter);
    original_fe_next[LF_FREELIST_POINTER] = fs->top[LF_FREELIST_POINTER];
    original_fe_next[LF_FREELIST_COUNTER] = fs->top[LF_FREELIST_COUNTER];
    do {
        fe_local[LF_FREELIST_POINTER]->next[LF_FREELIST_POINTER] = original_fe_next[LF_FREELIST_POINTER];
        fe_local[LF_FREELIST_POINTER]->next[LF_FREELIST_COUNTER] = original_fe_next[LF_FREELIST_COUNTER];
    } while (0 == abstraction_dcas((volatile atom_t *) fs->top, (atom_t *) fe_local, (atom_t *) original_fe_next));
    return;
}

void *freelist_get_data(struct freelist_element *fe, void **data) {
    assert(NULL != fe);
    LF_BARRIER_LOAD;
    if (NULL != data)
        *data = fe->data;
    return (fe->data);
}

void freelist_set_data(struct freelist_element *fe, void *data) {
    assert(NULL != fe);
    fe->data = data;
    LF_BARRIER_STORE;
    return;
}

void freelist_query(struct freelist_state *fs, enum freelist_query_type query_type, void *input, void *output) {
    assert(NULL != fs);
    assert(NULL != output);
    LF_BARRIER_LOAD;
    switch (query_type) {
        case FREELIST_QUERY_ELEMENT_COUNT:
            assert(NULL == input);
            *(atom_t *) output = fs->element_count;
            break;
        case FREELIST_QUERY_VALIDATE:
            freelist_internal_validate(fs, (struct validation_info *) input, (enum data_structure_validity *) output);
            break;
    }
    return;
}

void
freelist_internal_validate(struct freelist_state *fs, struct validation_info *vi, enum data_structure_validity *fv) {
    struct freelist_element *fe, *fe_slow, *fe_fast;
    atom_t element_count = 0;
    assert(fs != NULL);
    assert(NULL != fv);
    *fv = VALIDITY_VALID;
    fe_slow = fe_fast = (struct freelist_element *) fs->top[LF_FREELIST_POINTER];

    if (fe_slow != NULL) {
        do {
            fe_slow = fe_slow->next[LF_FREELIST_POINTER];
            if (fe_fast != NULL)
                fe_fast = fe_fast->next[LF_FREELIST_POINTER];
            if (fe_fast != NULL)
                fe_fast = fe_fast->next[LF_FREELIST_POINTER];
        } while (fe_slow != NULL AND fe_fast != fe_slow);
    }

    if (fe_fast != NULL AND fe_slow != NULL AND fe_fast == fe_slow)
        *fv = VALIDITY_INVALID_LOOP;

    if (*fv == VALIDITY_VALID AND vi != NULL) {
        fe = (struct freelist_element *) fs->top[LF_FREELIST_POINTER];

        while (fe != NULL) {
            element_count++;
            fe = (struct freelist_element *) fe->next[LF_FREELIST_POINTER];
        }

        if (element_count < vi->min_element)
            *fv = VALIDITY_INVALID_MISSING_ELEMENTS;
        if (element_count > vi->max_element)
            *fv = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
    }
    return;
}
