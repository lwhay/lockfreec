#ifndef LF_SLIST_H
#define LF_SLIST_H

#include "lf_basic.h"

#define SLIST_DATA  0
#define SLIST_FLAG  1

#define SLIST_NO_FLAGS      0x0
#define SLIST_FLAG_DELETED  0x1

#pragma pack(push, LF_ALIGN_SINGLE_POINTER)
struct slist_state {
    struct slist_element *volatile head;
    void (*delete_func)(void *data, void *state);
    void *state;
};
#pragma pack(pop)

#pragma pack(push, LF_ALIGN_DOUBLE_POINTER)

struct slist_element {
    void *volatile data_flag[2];
    struct slist_element *volatile next;
};
#pragma pack(pop)

void slist_internal_init_slist(struct slist_state *ss, void (*delete_func)(void *data, void *state), void *state);
void slist_internal_link_head(struct slist_state *ss, struct slist_element *volatile se);
void slist_internal_link_after(struct slist_element *volatile ase, struct slist_element *volatile se);
void slist_internal_moveto_first(struct slist_element **se);
#endif // LF_SLIST_H
