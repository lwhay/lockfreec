#ifndef LF_STACK_H
#define LF_STACK_H
#include "lf_basic.h"

#define STACK_POINTER   0
#define STACK_COUNTER   1
#define STACK_PAC_SIZE  2
#pragma pack(push, LF_ALIGN_DOUBLE_POINTER)
struct stack_state {
    struct stack_element *volatile top[STACK_PAC_SIZE];
    atom_t aba_counter;
    struct freelist_state *fs;
};

struct stack_element {
    struct stack_element *next[STACK_PAC_SIZE];
    struct freelist_element *fe;
    void* data;
};
#pragma pack(pop)
int stack_internal_freelist_init_function(void **data, void *state);
void stack_internal_freelist_delete_function(void *data, void *state);

void stack_internal_new_element_from_freelist(struct stack_state *ss, struct stack_element *se[STACK_PAC_SIZE], void *data);
void stack_internal_new_element(struct stack_state *ss, struct stack_element *se[STACK_PAC_SIZE], void *data);
void stack_internal_init_element(struct stack_state *ss, struct stack_element *se[STACK_PAC_SIZE], struct freelist_element *fe, void *data);

void stack_internal_push(struct stack_state *ss, struct stack_element *se[STACK_PAC_SIZE]);
void stack_internal_validate(struct stack_state *ss, struct validation_info *vi, enum data_structure_validity *sv, enum data_structure_validity *fv);

#endif // LF_STACK_H
