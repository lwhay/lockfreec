//
// Created by Michael on 2019-02-26.
//

#ifndef LOCKFREEC_STM_H
#define LOCKFREEC_STM_H

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uintptr_t stm_word_t;

typedef struct r_entry {                  /* Read set entry */
    stm_word_t version;                   /* Version read */
    volatile stm_word_t *lock;            /* Pointer to lock (for fast access) */
} r_entry_t;

typedef struct r_set {                    /* Read set */
    r_entry_t *entries;                   /* Array of entries */
    int nb_entries;                       /* Number of entries */
    int size;                             /* Size of array */
} r_set_t;

typedef struct w_entry {
    volatile stm_word_t *addr;            /* Address written */
    stm_word_t value;                     /* New (write-back) or old (write-through) value */
    stm_word_t mask;                      /* Write mask */
    stm_word_t version;                   /* Version overwritten */
    volatile stm_word_t *lock;            /* Pointer to lock (for fast access) */
    struct w_entry *next;                 /* Next address covered by same lock (if any) */
} w_entry_t;

typedef struct w_set {                  /* Write set */
    w_entry_t *entries;                   /* Array of entries */
    int nb_entries;                       /* Number of entries */
    int size;                             /* Size of array */
    int reallocate;                       /* Reallocate on next start */
} w_set_t;

#endif //LOCKFREEC_STM_H
