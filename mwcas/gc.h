//
// Created by Michael on 2018/12/20.
//

#ifndef LOCKFREEC_GC_H
#define LOCKFREEC_GC_H

#include <thread>
#include "ebr.h"

#define SPINLOCK_BACKOFF_MIN    4
#define SPINLOCK_BACKOFF_MAX    128
#if defined(__X86_64__) || defined(__i386__)
#define SPINLOCK_BACKOFF_HOOK __asm volatile("pause" ::: "memory")
#else
#define SPINLOCK_BACKOFF_HOOK
#endif
#define SPINLOCK_BACKOFF(count)     \
do {        \
    for (int __i = (count); __i != 0; __i--) {      \
        SPINLOCK_BACKOFF_HOOK;      \
    } \
    if ((count) < SPINLOCK_BACKOFF_MAX)  \
        (count) += (count); \
} while (0);

typedef struct gc_entry {
    gc_entry *next;
} gc_entry_t;

typedef void (*gc_func_t)(gc_entry_t *, void *);

typedef struct gc {
    gc_entry_t *limbo;
    gc_entry_t *epoch_list[EBR_EPOCHS];
    ebr_t *ebr;
    off_t entry_off;
    gc_func_t reclaim;
    void *arg;
} gc_t;

static void gc_default_reclaim(gc_entry_t *entry, void *arg) {
    gc_t *gc = (gc_t *) arg;
    const off_t off = gc->entry_off;
    void *obj;
    while (entry) {
        obj = (void *) ((uintptr_t) entry - off);
        entry = entry->next;
        free(obj);
    }
}

gc_t *gc_create(unsigned off, gc_func_t reclaim, void *arg) {
    gc_t *gc;
    if ((gc = (gc_t *) calloc(1, sizeof(gc_t))) == nullptr) {
        return nullptr;
    }
    gc->ebr = ebr_create();
    if (!gc->ebr) {
        free(gc);
        return nullptr;
    }
    gc->entry_off = off;
    if (reclaim) {
        gc->reclaim = reclaim;
        if (arg != nullptr) {
            gc->arg = arg;
        } else {
            gc->arg = gc;
        }
    } else {
        gc->reclaim = gc_default_reclaim;
        gc->arg = gc;
    }
    return gc;
}

void *gc_register(gc_t *gc) {
    ebr_register(gc->ebr);
}

void gc_crit_enter(gc_t *gc) {
    ebr_enter(gc->ebr);
}

void gc_crit_exit(gc_t *gc) {
    ebr_exit(gc->ebr);
}

void gc_limbo(gc_t *gc, void *obj) {
    gc_entry_t *ent = (gc_entry_t *) ((uintptr_t) obj + gc->entry_off);
    gc_entry_t *head;
    do {
        head = gc->limbo;
        ent->next = head;
    } while (CAS((uint64_t *) &gc->limbo, (uint64_t) ent, (uint64_t) head) != (uint64_t) head);
}

void gc_cycle(gc_t *gc) {
    unsigned count = EBR_EPOCHS, gc_epoch, staging_epoch;
    ebr_t *ebr = gc->ebr;
    gc_entry_t *gc_list;
    next:
    if (!ebr_sync(ebr, &gc_epoch)) {
        return;
    }
    staging_epoch = ebr_staging_epoch(ebr);
    gc->epoch_list[staging_epoch] = (gc_entry_t *) EXCHANGE((uint64_t *) &gc->limbo, NULL);

    gc_list = gc->epoch_list[gc_epoch];
    if (!gc_list && count--) {
        goto next;
    }
    gc->reclaim(gc_list, gc->arg);
    gc->epoch_list[gc_epoch] = nullptr;
}

void gc_full(gc_t *gc, unsigned msec_retry) {
    unsigned count = SPINLOCK_BACKOFF_MIN;
    bool done;
    again:
    gc_cycle(gc);
    done = true;
    for (unsigned i = 0; i < EBR_EPOCHS; i++) {
        if (gc->epoch_list[i]) {
            done = false;
            break;
        }
    }
    if (!done || gc->limbo) {
        if (count < SPINLOCK_BACKOFF_MAX) {
            SPINLOCK_BACKOFF(count);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(msec_retry));
        }
        goto again;
    }
}

#endif //LOCKFREEC_GC_H
