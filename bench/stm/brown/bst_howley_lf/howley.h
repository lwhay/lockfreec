/*   
 *   File: bst_howley.c
 *   Author: Balmau Oana <oana.balmau@epfl.ch>, 
 *  	     Zablotchi Igor <igor.zablotchi@epfl.ch>, 
 *  	     Tudor David <tudor.david@epfl.ch>
 *   Description: Shane V Howley and Jeremy Jones. 
 *   A non-blocking internal binary search tree. SPAA 2012
 *   bst_howley.c is part of ASCYLIB
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
 * Some changes by Trevor:
 * - The memory reclamation in the ASCYLIB implementation was wrong.
 *   I believe it leaked AND segfaulted. It is now correct.
 *   (Howley et al. didn't say how to reclaim at all.)
 * - There was a subtle bug with some incorrect NULL that should have been NULL_NODEPTR.
 * - Fixed incorrect volatile usage.
 * - Added proper padding on data structure globals (root, etc)
 *   to avoid false sharing with the test harness / enclosing program.
 * - Fixed value types so they don't need to be numeric.
 * - Fixed a bug that corrupted inserted numeric values (not keys, but values)
 *   that are smaller than 4.
 */

/* 
 * File:   howley.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 1, 2017, 10:57 AM
 */

#ifndef HOWLEY_H
#define HOWLEY_H

#include "record_manager.h"

#define USE_PADDING
#define LARGE_DES

//Encoded in the operation pointer
#define STATE_OP_NONE 0
#define STATE_OP_MARK 1
#define STATE_OP_CHILDCAS 2
#define STATE_OP_RELOCATE 3

//In the relocate_op struct
#define STATE_OP_ONGOING 0
#define STATE_OP_SUCCESSFUL 1
#define STATE_OP_FAILED 2

//States for the result of a search operation
#define FOUND 0x0
#define NOT_FOUND_L 0x1
#define NOT_FOUND_R 0x2
#define ABORT 0x3

#define GETFLAG(ptr) (((uint64_t) (ptr)) & 3)
#define FLAG(ptr, flag) ((operation_t<skey_t, sval_t> *) ((((uint64_t) (ptr)) & 0xfffffffffffffffc) | (flag)))
#define UNFLAG(ptr) ((operation_t<skey_t, sval_t> *) (((uint64_t) (ptr)) & 0xfffffffffffffffc))
//Last bit of the node pointer will be set to 1 if the pointer is null 
#define ISNULL(node) (((node) == NULL) || (((uint64_t) (node)) & 1))
#define SETNULL(node) ((node_t<skey_t, sval_t> *) ((((uintptr_t) (node)) & 0xfffffffffffffffe) | 1))
#define NULL_NODEPTR ((node_t<skey_t, sval_t> *) 1)

template<typename skey_t, typename sval_t>
struct node_t;

template<typename skey_t, typename sval_t>
union operation_t;

template<typename skey_t, typename sval_t>
struct child_cas_op_t {
    bool is_left;
    node_t<skey_t, sval_t> *expected;
    node_t<skey_t, sval_t> *update;

};

template<typename skey_t, typename sval_t>
struct relocate_op_t {
    int volatile state; // initialize to ONGOING every time a relocate operation is created
    node_t<skey_t, sval_t> *dest;
    operation_t<skey_t, sval_t> *dest_op;
    skey_t remove_key;
    sval_t remove_value;
    skey_t replace_key;
    sval_t replace_value;

};

template<typename skey_t, typename sval_t>
struct node_t {
    skey_t key;
    sval_t value;
    operation_t<skey_t, sval_t> *volatile op;
    node_t<skey_t, sval_t> *volatile left;
    node_t<skey_t, sval_t> *volatile right;
#ifdef USE_PADDING
    volatile char pad[64 - sizeof(key) - sizeof(value) - sizeof(op) - sizeof(left) - sizeof(right)];
#endif
};

template<typename skey_t, typename sval_t>
union operation_t {
    child_cas_op_t<skey_t, sval_t> child_cas_op;
    relocate_op_t<skey_t, sval_t> relocate_op;
#if defined(LARGE_DES)
    uint8_t padding[112]; //unique for both TPCC and YCSB
#else
    uint8_t padding[64];
#endif
};

template<typename sval_t>
struct find_result {
    sval_t val;
    int code;
};

template<typename skey_t, typename sval_t, class RecMgr>
class howley {
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
    int init[MAX_THREADS_POW2] = {
            0,}; // this suffers from false sharing, but is only touched once per thread! so no worries.
    PAD;

    node_t<skey_t, sval_t> *
    create_node(const int tid, skey_t key, sval_t value, node_t<skey_t, sval_t> *left, node_t<skey_t, sval_t> *right) {
        auto result = recmgr->template allocate<node_t<skey_t, sval_t>>(tid);
        if (result == NULL) {
            perror("malloc in bst create node");
            exit(1);
        }
        result->key = key;
        result->value = value;
        result->op = NULL;
        result->left = left;
        result->right = right;
        return result;
    }

    operation_t<skey_t, sval_t> *alloc_op(const int tid) {
        auto result = recmgr->template allocate<operation_t<skey_t, sval_t>>(tid);
        if (result == NULL) {
            perror("malloc in bst create node");
            exit(1);
        }
        return result;
    }

    void bst_help_child_cas(const int tid, operation_t<skey_t, sval_t> *op, node_t<skey_t, sval_t> *dest);

    bool bst_help_relocate(const int tid, operation_t<skey_t, sval_t> *op, node_t<skey_t, sval_t> *pred,
                           operation_t<skey_t, sval_t> *pred_op, node_t<skey_t, sval_t> *curr);

    void bst_help_marked(const int tid, node_t<skey_t, sval_t> *pred, operation_t<skey_t, sval_t> *pred_op,
                         node_t<skey_t, sval_t> *curr);

    void bst_help(const int tid, node_t<skey_t, sval_t> *pred, operation_t<skey_t, sval_t> *pred_op,
                  node_t<skey_t, sval_t> *curr, operation_t<skey_t, sval_t> *curr_op);

    find_result<sval_t>
    bst_find(const int tid, skey_t k, node_t<skey_t, sval_t> **pred, operation_t<skey_t, sval_t> **pred_op,
             node_t<skey_t, sval_t> **curr, operation_t<skey_t, sval_t> **curr_op, node_t<skey_t, sval_t> *aux_root,
             node_t<skey_t, sval_t> *root);

public:

    howley(const int _NUM_THREADS, const skey_t &_KEY_MIN, const skey_t &_KEY_MAX, const sval_t &_VALUE_RESERVED,
           unsigned int id)
            : idx_id(id), NUM_THREADS(_NUM_THREADS), KEY_MIN(_KEY_MIN), KEY_MAX(_KEY_MAX), NO_VALUE(_VALUE_RESERVED),
              recmgr(new RecMgr(NUM_THREADS)) {
        const int tid = 0;
        initThread(tid);

        recmgr->endOp(tid); // enter an initial quiescent state.

        root = create_node(tid, KEY_MAX, NO_VALUE, NULL_NODEPTR, NULL_NODEPTR);
    }

    ~howley() {
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

    sval_t bst_add(const int tid, skey_t k, sval_t v);

    sval_t bst_remove(const int tid, skey_t k);

    node_t<skey_t, sval_t> *get_root() {
        return root;
    }

    RecMgr *debugGetRecMgr() {
        return recmgr;
    }
};

#endif /* HOWLEY_H */

