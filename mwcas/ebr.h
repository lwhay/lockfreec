//
// Created by Michael on 2018/12/20.
//

#ifndef LOCKFREEC_EBR_H
#define LOCKFREEC_EBR_H

#include <cassert>
#include <atomic>
#include <stdio.h>
#include "rel_ptr.h"

#if defined(__MINGW64__) || defined(__CYGWIN__)

#include <string.h>

#endif

struct ebr;
typedef struct ebr ebr_t;

#define EBR_EPOCHS  3

#define ACTIVE_FLAG (0x80000000U)

typedef struct ebr_tls {
    unsigned local_epoch;
    struct ebr_tls *next;
} ebr_tls_t;

thread_local ebr_tls_t *local_ebr = nullptr;

struct ebr {
    unsigned global_epoch;
    ebr_tls_t *list;
};

ebr_t *ebr_create(void) {
    ebr_t *ebr;
    printf("-");
    if ((ebr = (ebr_t *) calloc(1, sizeof(ebr_t))) == nullptr) {
        return nullptr;
    }
    return ebr;
}

void ebr_destroy(ebr_t *ebr) {
    free(ebr);
}

int ebr_register(ebr_t *ebr) {
    ebr_tls_t *head;
    if (local_ebr) {
        return 0;
    }
    if ((local_ebr = (ebr_tls_t *) malloc(sizeof(ebr_tls_t))) == nullptr) {
        return -1;
    }
    memset(local_ebr, 0, sizeof(ebr_tls_t));
    uint64_t r;
    do {
        head = ebr->list;
        local_ebr->next = head;
        r = CAS((uint64_t *) &ebr->list, (uint64_t) local_ebr, (uint64_t) head);
    } while (r != (uint64_t) head);
    return 0;
}

void ebr_enter(ebr_t *ebr) {
    assert(local_ebr);
    local_ebr->local_epoch = ebr->global_epoch | ACTIVE_FLAG;
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

void ebr_exit(ebr_t *) {
    ebr_tls_t *t;
    t = local_ebr;
    assert(t != nullptr);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    assert(t->local_epoch & ACTIVE_FLAG);
    t->local_epoch = 0;
}

unsigned ebr_staging_epoch(ebr_t *ebr) {
    return ebr->global_epoch;
}

unsigned ebr_gc_epoch(ebr_t *ebr) {
    return (ebr->global_epoch + 1) % EBR_EPOCHS;
}

bool ebr_sync(ebr_t *ebr, unsigned *gc_epoch) {
    unsigned epoch;
    ebr_tls_t *t;
    epoch = ebr->global_epoch;
    std::atomic_thread_fence(std::memory_order_seq_cst);

    t = ebr->list;
    while (t) {
        const unsigned local_epoch = t->local_epoch;
        const bool active = (local_epoch & ACTIVE_FLAG) != 0;

        if (active && (local_epoch != (epoch | ACTIVE_FLAG))) {
            *gc_epoch = ebr_gc_epoch(ebr);
            return false;
        }
        t = t->next;
    }
    ebr->global_epoch = (epoch + 1) % EBR_EPOCHS;
    *gc_epoch = ebr_gc_epoch(ebr);
}

#endif //LOCKFREEC_EBR_H
