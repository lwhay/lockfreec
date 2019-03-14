/*   
 *   File: bst-drachsler.c
 *   Author: Tudor David <tudor.david@epfl.ch>
 *   Description: Dana Drachsler, Martin Vechev, and Eran Yahav. 
 *   Practical Concurrent Binary Search Trees via Logical Ordering. PPoPP 2014.
 *   bst-drachsler.c is part of ASCYLIB
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>,
 * 	     	      Tudor David <tudor.david@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * ASCYLIB is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* 
 * File:   drachsler.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 7, 2017, 3:25 PM
 */

#ifndef DANA_H
#define DANA_H

#include <pthread.h>
#include "record_manager.h"

#define FIELDS_ORDER
#define SPIN_LOCK
#define USE_PADDING

#ifndef SPIN_LOCK
#error only spin lock is currently supported
#endif

typedef pthread_spinlock_t ptlock_t;
#define PTLOCK_SIZE         sizeof(ptlock_t)
#define INIT_LOCK(lock)     pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE);
#define DESTROY_LOCK(lock)  pthread_spin_destroy(lock)
#define LOCK(lock)          pthread_spin_lock(lock)
#define UNLOCK(lock)        pthread_spin_unlock(lock)
#define TRYLOCK(lock)       pthread_spin_trylock(lock)

#ifdef BASELINE //make sure that small key is defined and no padding
#ifdef USE_PADDING
#undef USE_PADDING
#endif
#ifdef FIELDS_ORDER
#undef FIELDS_ORDER
#endif
#endif


#define ALIGNED(N) __attribute__ ((aligned (N)))

typedef uint8_t bool_t;

template<typename skey_t, typename sval_t>
struct node_t {
#ifdef USE_PADDING
    union {
        volatile char pad[192];
        struct {
#endif
#ifdef FIELDS_ORDER
            skey_t key;
#ifndef NO_VOLATILE
            node_t<skey_t, sval_t> *volatile left;
            node_t<skey_t, sval_t> *volatile right;
            node_t<skey_t, sval_t> *volatile succ;
            node_t<skey_t, sval_t> *volatile pred;
#else
            volatile node_t<skey_t, sval_t>* left;
            volatile node_t<skey_t, sval_t>* right;
            volatile node_t<skey_t, sval_t>* succ;
            volatile node_t<skey_t, sval_t>* pred;
#endif
            bool_t mark;
            sval_t value;
            node_t<skey_t, sval_t> *volatile parent;
            ptlock_t tree_lock;
            ptlock_t succ_lock;
#else
            volatile node_t<skey_t, sval_t>* left;
            volatile node_t<skey_t, sval_t>* right;
            volatile node_t<skey_t, sval_t>* parent;
            volatile node_t<skey_t, sval_t>* succ;
            volatile node_t<skey_t, sval_t>* pred;
            ptlock_t tree_lock;
            ptlock_t succ_lock;
            skey_t key;
            sval_t value;
            bool_t mark;
#endif
#ifdef USE_PADDING
        };
    };
#endif
#ifdef BASELINE
    char padding[96-5*sizeof(uintptr_t)-2*sizeof(ptlock_t)-sizeof(sval_t)-sizeof(skey_t)-sizeof(bool_t)];
#endif
};

template<typename skey_t, typename sval_t, class RecMgr>
class drachsler {
private:
    PAD;
    const unsigned int idx_id;
    PAD;
    node_t<skey_t, sval_t> *root;
    PAD;
    const int NUM_THREADS;
    const skey_t KEY_MIN;
    const skey_t KEY_MAX;
    const sval_t NO_VALUE;
    PAD;
    RecMgr *const recmgr;
    PAD;
    int init[MAX_THREADS_POW2] = {0,};
    PAD;

    node_t<skey_t, sval_t> *create_node(const int tid, skey_t k, sval_t value, int initializing);

    node_t<skey_t, sval_t> *initialize_tree(const int tid);

    node_t<skey_t, sval_t> *bst_search(const int tid, skey_t key);

    node_t<skey_t, sval_t> *choose_parent(const int tid, node_t<skey_t, sval_t> *pred, node_t<skey_t, sval_t> *succ,
                                          node_t<skey_t, sval_t> *first_cand);

    void insert_to_tree(const int tid, node_t<skey_t, sval_t> *parent, node_t<skey_t, sval_t> *new_node);

    node_t<skey_t, sval_t> *lock_parent(const int tid, node_t<skey_t, sval_t> *node);

    bool_t acquire_tree_locks(const int tid, node_t<skey_t, sval_t> *n);

    void remove_from_tree(const int tid, node_t<skey_t, sval_t> *n, bool_t has_two_children);

    void update_child(const int tid, node_t<skey_t, sval_t> *parent, node_t<skey_t, sval_t> *old_ch,
                      node_t<skey_t, sval_t> *new_ch);

public:

    drachsler(const int _NUM_THREADS, const skey_t &_KEY_MIN, const skey_t &_KEY_MAX, const sval_t &_VALUE_RESERVED,
              unsigned int id)
            : NUM_THREADS(_NUM_THREADS), KEY_MIN(_KEY_MIN), KEY_MAX(_KEY_MAX), NO_VALUE(_VALUE_RESERVED), idx_id(id),
              recmgr(new RecMgr(NUM_THREADS)) {
        const int tid = 0;
        initThread(tid);

        recmgr->endOp(tid); // enter an initial quiescent state.
        root = initialize_tree(tid);
    }

    ~drachsler() {
        recmgr->printStatus();
        delete recmgr;
    }

    void initThread(const int tid) {
        if (init[tid]) return;
        else init[tid] = !init[tid];
        recmgr->initThread(tid);
    }

    void deinitThread(const int tid) {
        if (!init[tid]) return;
        else init[tid] = !init[tid];
        recmgr->deinitThread(tid);
    }

    sval_t bst_contains(const int tid, skey_t k);

    sval_t bst_insert(const int tid, skey_t k, sval_t v, bool onlyIfAbsent = true);

    sval_t bst_remove(const int tid, skey_t key);

    node_t<skey_t, sval_t> *get_root() {
        return root;
    }

    RecMgr *debugGetRecMgr() {
        return recmgr;
    }
};

#endif /* DANA_H */

