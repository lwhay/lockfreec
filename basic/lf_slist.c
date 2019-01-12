#include "lf_slist.h"

void slist_internal_link_head(struct slist_state *ss, struct slist_element *volatile se) {
    LF_ALIGN(LF_ALIGN_SINGLE_POINTER) struct slist_element *se_next;
    assert(NULL != ss);
    assert(NULL != se);
    LF_BARRIER_LOAD;
    se_next = ss->head;
    do {
        se->next = se_next;
    } while (se->next != (se_next = (struct slist_element *) abstraction_cas((volatile atom_t *) &ss->head, (atom_t) se,
                                                                             (atom_t) se->next)));
    return;
}

void slist_internal_link_after(struct slist_element *volatile ase, struct slist_element *volatile se) {
    LF_ALIGN(LF_ALIGN_SINGLE_POINTER) struct slist_element *se_prev, *se_next;
    assert(ase != NULL);
    assert(se != NULL);
    LF_BARRIER_LOAD;
    se_prev = (struct slist_element *) ase;
    se_next = se_prev->next;
    do {
        se->next = se_next;
    } while (se_next !=
             (se_next = (struct slist_element *) abstraction_cas((volatile atom_t *) &se_prev->next, (atom_t) se,
                                                                 (atom_t) se->next)));
    return;
}

int slist_new(struct slist_state **ss, void (*delete_func)(void *data, void *state), void *state) {
    int rv = 0;
    assert(NULL != ss);
    *ss = (struct slist_state *) aligned_malloc(sizeof(struct slist_state), LF_ALIGN_SINGLE_POINTER);
    if (NULL != *ss) {
        slist_internal_init_slist(*ss, delete_func, state);
        rv = 1;
    }
    LF_BARRIER_STORE;
    return (rv);
}

void slist_delete(struct slist_state *ss) {
    slist_single_threaded_physically_delete_all_elements(ss);
    aligned_free(ss);
    return;
}

int slist_logically_delete_element(struct slist_state *ss, struct slist_element *se) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) void *volatile data_flag[2], *volatile new_data_flag[2];
    unsigned char cas_rv = 0;
    assert(NULL != ss);
    assert(NULL != se);
    LF_BARRIER_LOAD;
    data_flag[SLIST_DATA] = se->data_flag[SLIST_DATA];
    data_flag[SLIST_FLAG] = se->data_flag[SLIST_FLAG];
    do {
        new_data_flag[SLIST_DATA] = data_flag[SLIST_DATA];
        new_data_flag[SLIST_FLAG] = (void *) ((atom_t) data_flag[SLIST_FLAG] | SLIST_FLAG_DELETED);
    } while (!((atom_t) data_flag[SLIST_FLAG] & SLIST_FLAG_DELETED) AND 0 == (cas_rv = abstraction_dcas(
            (volatile atom_t *) se->data_flag, (atom_t *) new_data_flag, (atom_t *) data_flag)));
    if (1 == cas_rv)
        if (ss->delete_func != NULL)
            ss->delete_func((void *) data_flag[SLIST_DATA], ss->state);
    return (cas_rv);
}

void slist_single_threaded_physically_delete_all_elements(struct slist_state *ss) {
    struct slist_element *volatile se, *volatile se_temp;
    LF_BARRIER_LOAD;
    se = ss->head;
    while (se != NULL) {
        if (ss->delete_func != NULL)
            ss->delete_func((void *) se->data_flag[SLIST_DATA], ss->state);
        se_temp = se;
        se = se->next;
        aligned_free((void *) se_temp);
    }
    slist_internal_init_slist(ss, ss->delete_func, ss->state);
    LF_BARRIER_STORE;
    return;
}

int slist_get_data(struct slist_element *se, void **data) {
    int rv = 1;
    assert(NULL != se);
    assert(NULL != data);
    LF_BARRIER_LOAD;
    *data = (void *) se->data_flag[SLIST_DATA];
    if ((atom_t) se->data_flag[SLIST_FLAG] & SLIST_FLAG_DELETED)
        rv = 0;
    return (rv);
}

int slist_set_data(struct slist_element *se, void *data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) void *data_flag[2], *new_data_flag[2];
    int rv = 1;
    assert(NULL != se);
    LF_BARRIER_LOAD;
    data_flag[SLIST_DATA] = se->data_flag[SLIST_DATA];
    data_flag[SLIST_FLAG] = se->data_flag[SLIST_FLAG];
    new_data_flag[SLIST_DATA] = data;
    do {
        new_data_flag[SLIST_FLAG] = data_flag[SLIST_FLAG];
    } while (!((atom_t) data_flag[SLIST_FLAG] & SLIST_FLAG_DELETED) AND 0 == abstraction_dcas(
            (volatile atom_t *) se->data_flag, (atom_t *) new_data_flag, (atom_t *) data_flag));
    if ((atom_t) data_flag[SLIST_FLAG] & SLIST_FLAG_DELETED)
        rv = 0;
    LF_BARRIER_STORE;
    return (rv);
}

struct slist_element *slist_get_head(struct slist_state *ss, struct slist_element **se) {
    assert(NULL != ss);
    assert(NULL != se);
    LF_BARRIER_LOAD;
    *se = (struct slist_element *) ss->head;
    slist_internal_moveto_first(se);
    return (*se);
}

struct slist_element *slist_get_next(struct slist_element *se, struct slist_element **nse) {
    assert(NULL != se);
    assert(NULL != nse);
    LF_BARRIER_LOAD;
    *nse = (struct slist_element *) se->next;
    slist_internal_moveto_first(nse);
    return (*nse);
}

struct slist_element *slist_pick_head(struct slist_state *ss, struct slist_element **se) {
    assert(NULL != ss);
    assert(NULL != se);
    if (*se == NULL)
        slist_get_head(ss, se);
    else
        slist_get_next(*se, se);
    return (*se);
}

void slist_internal_moveto_first(struct slist_element **se) {
    assert(NULL != se);
    while (*se != NULL AND (atom_t) (*se)->data_flag[SLIST_FLAG] & SLIST_FLAG_DELETED)
        (*se) = (struct slist_element *) (*se)->next;
    return;
}

#pragma warning(disable : 4100)

void slist_use(struct slist_state *ss) {
    assert(NULL != ss);
    LF_BARRIER_LOAD;
    return;
}

#pragma warning(default : 4100)

void slist_internal_init_slist(struct slist_state *ss, void (*delete_func)(void *, void *), void *state) {
    assert(NULL != ss);
    ss->head = NULL;
    ss->delete_func = delete_func;
    ss->state = state;
    return;
}

struct slist_element *slist_new_head(struct slist_state *ss, void *data) {
    LF_ALIGN(LF_ALIGN_SINGLE_POINTER) struct slist_element *volatile se;
    assert(NULL != ss);
    se = (struct slist_element *) aligned_malloc(sizeof(struct slist_element), LF_ALIGN_DOUBLE_POINTER);
    if (NULL != se) {
        se->data_flag[SLIST_DATA] = data;
        se->data_flag[SLIST_FLAG] = SLIST_NO_FLAGS;
        slist_internal_link_head(ss, se);
    }
    return ((struct slist_element *) se);
}

struct slist_element *slist_new_next(struct slist_element *se, void *data) {
    LF_ALIGN(LF_ALIGN_SINGLE_POINTER) struct slist_element *volatile se_next;
    assert(NULL != se);
    se_next = (struct slist_element *) aligned_malloc(sizeof(struct slist_element), LF_ALIGN_DOUBLE_POINTER);
    if (NULL != se_next) {
        se_next->data_flag[SLIST_DATA] = data;
        se_next->data_flag[SLIST_FLAG] = SLIST_NO_FLAGS;
        slist_internal_link_after(se, se_next);
    }
    return ((struct slist_element *) se_next);
}
