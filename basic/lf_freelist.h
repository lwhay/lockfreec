#ifndef LF_FREELIST_H
#define LF_FREELIST_H
#include "lf_basic.h"

#define LF_FREELIST_POINTER     0
#define LF_FREELIST_COUNTER     1
#define LF_FREELIST_PAC_SIZE    2

// Freelist structures
#pragma pack(push, LF_ALIGN_DOUBLE_POINTER)

struct freelist_state {
    struct  freelist_element* volatile top[LF_FREELIST_PAC_SIZE];
    int     (*init_func)(void** data, void* state);
    void*   state;
    atom_t  aba_counter, element_count;
};

struct freelist_element {
    struct  freelist_element *next[LF_FREELIST_PAC_SIZE];
    void*   data;
};

#pragma pack(pop)

atom_t  freelist_internal_new_element(struct freelist_state* fs, struct freelist_element** fe);
void    freelist_internal_validate(struct freelist_state* fs, struct validation_info* vi, enum data_structure_validity* fv);
#endif // LF_FREELIST_H
