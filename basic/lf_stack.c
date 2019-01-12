#include "lf_stack.h"

int stack_new(struct stack_state **ss, atom_t number_elements) {
    int rv = 0;
    assert(NULL != ss);
    *ss = (struct stack_state *)aligned_malloc(sizeof(struct stack_state), LF_ALIGN_DOUBLE_POINTER);
    if (NULL != ss) {
        freelist_new(&(*ss)->fs, number_elements, stack_internal_freelist_init_function, NULL);
        if ((*ss)->fs == NULL) {
            aligned_free(*ss);
            *ss = NULL;
        }
        if ((*ss)->fs != NULL) {
            (*ss)->top[STACK_POINTER] = NULL;
            (*ss)->top[STACK_COUNTER] = 0;
            (*ss)->aba_counter = 0;
            rv = 1;
        }
    }
    LF_BARRIER_STORE;
    return (rv);
}

void stack_delete(struct stack_state *ss, void (*delete_func)(void *data, void *state), void *state) {
    void *data;
    assert(NULL != ss);
    while (stack_pop(ss, &data))
        if (delete_func != NULL)
            delete_func(data, state);
    freelist_delete(ss->fs, stack_internal_freelist_delete_function, NULL);
    aligned_free(ss);
    return;
}

void stack_clear(struct stack_state *ss, void (*clear_func)(void *data, void *state), void *state) {
    void *data;
    assert(NULL != ss);
    while (stack_pop(ss, &data))
        if (clear_func != NULL)
            clear_func(data, state);
    return;
}

int stack_push(struct stack_state *ss, void *data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct stack_element *se[STACK_PAC_SIZE];
    assert(NULL != ss);
    stack_internal_new_element_from_freelist(ss, se, data);
    if (se[STACK_POINTER] == NULL)
        return (0);
    stack_internal_push(ss, se);
    return (1);
}

int stack_guaranteed_push(struct stack_state *ss, void *data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct stack_element *se[STACK_PAC_SIZE];
    assert(ss != NULL);
    stack_internal_new_element(ss, se, data);
    if (NULL == se[STACK_POINTER])
        return (0);
    stack_internal_push(ss, se);
    return (1);
}

void stack_internal_push(struct stack_state *ss, struct stack_element *se[STACK_PAC_SIZE]) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct stack_element *org_top[STACK_PAC_SIZE];
    assert(NULL != ss);
    assert(NULL != se);
    LF_BARRIER_LOAD;
    org_top[STACK_POINTER] = ss->top[STACK_POINTER];
    org_top[STACK_COUNTER] = ss->top[STACK_COUNTER];
    do {
        se[STACK_POINTER]->next[STACK_POINTER] = org_top[STACK_POINTER];
        se[STACK_POINTER]->next[STACK_COUNTER] = org_top[STACK_COUNTER];
    } while (0 == abstraction_dcas((volatile atom_t *)ss->top, (atom_t *)se, (atom_t *)org_top));
    return;
}

int stack_pop(struct stack_state *ss, void **data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct stack_element *se[STACK_PAC_SIZE];
    assert(NULL != ss);
    assert(data != NULL);
    LF_BARRIER_LOAD;
    se[STACK_POINTER] = ss->top[STACK_POINTER];
    se[STACK_COUNTER] = ss->top[STACK_COUNTER];
    do {
        if (se[STACK_POINTER] == NULL)
            return (0);
    } while (0 == abstraction_dcas((volatile atom_t *)ss->top, (atom_t *)se[STACK_POINTER]->next, (atom_t *)se));
    *data = se[STACK_POINTER]->data;
    freelist_push(ss->fs, se[STACK_POINTER]->fe);
    return (1);
}

void stack_query(struct stack_state *ss, enum stack_query_type query_type, void *input, void *output) {
    assert(NULL != ss);
    assert(NULL != output);
    LF_BARRIER_LOAD;
    switch (query_type) {
    case STACK_QUERY_ELEMENT_COUNT:
        assert(NULL != input);
        freelist_query(ss->fs, FREELIST_QUERY_ELEMENT_COUNT, NULL, output);
        break;
    case STACK_QUERY_VALIDATE:
        freelist_query(ss->fs, FREELIST_QUERY_VALIDATE, NULL, (enum data_structure_validity *)output);
        if (*(enum data_structure_validity *)output == VALIDITY_VALID)
            stack_internal_validate(ss, (struct validation_info *)input, (enum data_structure_validity *)output, ((enum data_structure_validity *)output) + 1);
        break;
    }
    return;
}

void stack_internal_validate(struct stack_state *ss, struct validation_info *vi, enum data_structure_validity *sv, enum data_structure_validity *fv) {
    struct stack_element *se, *se_slow, *se_fast;
    atom_t element_count = 0, total_elements;
    struct validation_info fvi;
    assert(NULL != ss);
    assert(NULL != sv);
    *sv = VALIDITY_VALID;
    se_slow = se_fast = (struct stack_element *)ss->top[STACK_POINTER];
    if (se_slow != NULL) {
        do {
            se_slow = se_slow->next[STACK_POINTER];
            if (se_fast != NULL)
                se_fast = se_fast->next[STACK_POINTER];
            if (se_fast != NULL)
                se_fast = se_fast->next[STACK_POINTER];
        } while (se_slow != NULL AND se_fast != se_slow);
    }
    if (se_fast != NULL AND se_slow != NULL AND se_fast == se_slow)
        *sv = VALIDITY_INVALID_LOOP;
    if (*sv == VALIDITY_VALID AND vi != NULL) {
        se = (struct stack_element *)ss->top[STACK_POINTER];
        while (se != NULL) {
            element_count++;
            se = (struct stack_element *)se->next[STACK_POINTER];
        }
        if (element_count < vi->min_element)
            *sv = VALIDITY_INVALID_MISSING_ELEMENTS;
        if (element_count > vi->max_element)
            *sv = VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
    }
    if (NULL != vi) {
        freelist_query(ss->fs, FREELIST_QUERY_ELEMENT_COUNT, NULL, (void *)&total_elements);
        fvi.min_element = total_elements - vi->max_element;
        fvi.max_element = total_elements - vi->min_element;
        freelist_query(ss->fs, FREELIST_QUERY_VALIDATE, (void *)&fvi, (void *)fv);
    }
    if (vi != NULL)
        freelist_query(ss->fs, FREELIST_QUERY_VALIDATE, NULL, (void *)fv);
    return;
}

#pragma warning(disable : 4100)
void stack_internal_freelist_delete_function(void *data, void *state) {
    assert(NULL != data);
    assert(NULL == state);
    aligned_free(data);
    return;
}
#pragma warning(default : 4100)

#pragma warning(disable : 4100)
void stack_use(struct stack_state *ss) {
    assert(ss != NULL);
    LF_BARRIER_LOAD;
    return;
}

#pragma warning(default : 4100)

#pragma warning(disable : 4100)
int stack_internal_freelist_init_function(void **data, void *state) {
    int rv = 0;
    assert(NULL != data);
    assert(NULL == state);
    *data = aligned_malloc(sizeof(struct stack_element), LF_ALIGN_DOUBLE_POINTER);
    if (*data != NULL)
        rv = 1;
    return (rv);
}

#pragma warning(default : 4100)

void stack_internal_new_element_from_freelist(struct stack_state *ss, struct stack_element *se[STACK_PAC_SIZE], void *data) {
    struct freelist_element *fe;
    assert(NULL != ss);
    assert(NULL != se);
    freelist_pop(ss->fs, &fe);
    if (NULL == fe)
        se[STACK_POINTER] = NULL;
    if (fe != NULL)
        stack_internal_init_element(ss, se, fe, data);
    return;
}

void stack_internal_new_element(struct stack_state *ss, struct stack_element *se[STACK_PAC_SIZE], void *data) {
    struct freelist_element *fe;
    assert(NULL != ss);
    assert(NULL != se);
    freelist_guaranteed_pop(ss->fs, &fe);
    if (fe == NULL)
        se[STACK_POINTER] = NULL;
    if (NULL != fe)
        stack_internal_init_element(ss, se, fe, data);
    return;
}

void stack_internal_init_element(struct stack_state *ss, struct stack_element *se[STACK_PAC_SIZE], struct freelist_element *fe, void *data) {
    assert(NULL != ss);
    assert(NULL != se);
    assert(NULL != fe);
    freelist_get_data(fe, (void **)&se[STACK_POINTER]);
    se[STACK_COUNTER] = (struct stack_element *)abstraction_increment((atom_t *)&ss->aba_counter);
    se[STACK_POINTER]->next[STACK_POINTER] = NULL;
    se[STACK_POINTER]->next[STACK_COUNTER] = 0;
    se[STACK_POINTER]->fe = fe;
    se[STACK_POINTER]->data = data;
    return;
}
