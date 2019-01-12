#ifndef LF_RINGBUFFER_H
#define LF_RINGBUFFER_H
#include "lf_basic.h"

#pragma pack(push, LF_ALIGN_DOUBLE_POINTER)
struct ringbuffer_state {
    struct queue_state *qs;
    struct freelist_state *fs;
};
#pragma pack(pop)

void ringbuffer_internal_validate(struct ringbuffer_state *rs, struct validation_info *vi, enum data_structure_validity *qv, enum data_structure_validity *fv);

#endif // LF_RINGBUFFER_H
