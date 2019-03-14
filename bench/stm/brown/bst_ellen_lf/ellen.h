/*   
 *   File: bst_ellen.c
 *   Author: Tudor David <tudor.david@epfl.ch>
 *   Description: non-blocking binary search tree
 *      based on "Non-blocking Binary Search Trees"
 *      F. Ellen et al., PODC 2010
 *   bst_ellen.c is part of ASCYLIB
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
 * File:   ellen.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 1, 2017, 3:56 PM
 */

#ifndef ELLEN_H
#define ELLEN_H

#include "record_manager.h"

#define STATE_CLEAN 0
#define STATE_DFLAG 1
#define STATE_IFLAG 2
#define STATE_MARK 3

#define GETFLAG(ptr) (((uint64_t) (ptr)) & 3)
#define FLAG(ptr, flag) (info_t<skey_t, sval_t> *) ((((uint64_t) (ptr)) & 0xfffffffffffffffc) | (flag))
#define UNFLAG(ptr) (info_t<skey_t, sval_t> *) (((uint64_t) (ptr)) & 0xfffffffffffffffc)

template<typename skey_t, typename sval_t>
union info_t;

template<typename skey_t, typename sval_t>
struct node_t;

template<typename skey_t, typename sval_t>
struct iinfo_t {
    node_t<skey_t, sval_t> *p;
    node_t<skey_t, sval_t> *new_internal;
    node_t<skey_t, sval_t> *l;
};

template<typename skey_t, typename sval_t>
struct dinfo_t {
    node_t<skey_t, sval_t> *gp;
    node_t<skey_t, sval_t> *p;
    node_t<skey_t, sval_t> *l;
    info_t<skey_t, sval_t> *pupdate;
};

template<typename skey_t, typename sval_t>
union info_t {
    iinfo_t<skey_t, sval_t> iinfo;
    dinfo_t<skey_t, sval_t> dinfo;
#if defined(LARGE_DES)
    uint8_t padding[128 - 4*sizeof(void *)];
#else
    uint8_t padding[64];
#endif
};

template<typename skey_t, typename sval_t>
struct node_t {
    skey_t key;
    sval_t value;
    info_t<skey_t, sval_t> *volatile update;
    node_t<skey_t, sval_t> *volatile left;
    node_t<skey_t, sval_t> *volatile right;
#ifdef USE_PADDING
    char pad[128 - sizeof(key) - sizeof(value) - sizeof(update) - sizeof(left) - sizeof(right)];
#endif
};

template<typename skey_t, typename sval_t, class RecMgr>
class ellen {
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

    bool bst_cas_child(const int tid, node_t<skey_t, sval_t> *parent, node_t<skey_t, sval_t> *old,
                       node_t<skey_t, sval_t> *nnode);

    void bst_help(const int tid, info_t<skey_t, sval_t> *u);

    void bst_help_marked(const int tid, info_t<skey_t, sval_t> *op);

    bool bst_help_delete(const int tid, info_t<skey_t, sval_t> *op);

    void bst_help_insert(const int tid, info_t<skey_t, sval_t> *op);

    node_t<skey_t, sval_t> *
    create_node(const int tid, skey_t key, sval_t value, node_t<skey_t, sval_t> *left, node_t<skey_t, sval_t> *right) {
        auto result = recmgr->template allocate<node_t<skey_t, sval_t>>(tid);
        if (result == NULL) setbench_error("out of memory");
        result->key = key;
        result->value = value;
        result->update = NULL;
        result->left = left;
        result->right = right;
        return result;
    }

    info_t<skey_t, sval_t> *
    create_iinfo_t(const int tid, node_t<skey_t, sval_t> *p, node_t<skey_t, sval_t> *ni, node_t<skey_t, sval_t> *l) {
        auto result = recmgr->template allocate<info_t<skey_t, sval_t>>(tid);
        if (result == NULL) setbench_error("out of memory");
        result->iinfo.p = p;
        result->iinfo.new_internal = ni;
        result->iinfo.l = l;
        return result;
    }

    info_t<skey_t, sval_t> *
    create_dinfo_t(const int tid, node_t<skey_t, sval_t> *gp, node_t<skey_t, sval_t> *p, node_t<skey_t, sval_t> *l,
                   info_t<skey_t, sval_t> *u) {
        auto result = recmgr->template allocate<info_t<skey_t, sval_t>>(tid);
        if (result == NULL) setbench_error("out of memory");
        result->dinfo.gp = gp;
        result->dinfo.p = p;
        result->dinfo.l = l;
        result->dinfo.pupdate = u;
        return result;
    }

public:

    ellen(const int _NUM_THREADS, const skey_t &_KEY_MIN, const skey_t &_KEY_MAX, const sval_t &_VALUE_RESERVED,
          unsigned int id)
            : idx_id(id), NUM_THREADS(_NUM_THREADS), KEY_MIN(_KEY_MIN), KEY_MAX(_KEY_MAX), NO_VALUE(_VALUE_RESERVED),
              recmgr(new RecMgr(NUM_THREADS)) {
        const int tid = 0;
        initThread(tid);

        recmgr->endOp(tid); // enter an initial quiescent state.

        auto i1 = create_node(tid, KEY_MAX, NO_VALUE, NULL, NULL);
        auto i2 = create_node(tid, KEY_MAX, NO_VALUE, NULL, NULL);
        root = create_node(tid, KEY_MAX, NO_VALUE, i1, i2);
    }

    ~ellen() {
        recmgr->printStatus();
        delete recmgr;
    }

    void printTree(node_t<skey_t, sval_t> *node, int depth) {
        //if (depth > 5) return;
        std::cout << "depth=" << depth << " key=" << node->key << std::endl;
        if (node->left) printTree(node->left, depth + 1);
        if (node->right) printTree(node->right, depth + 1);
    }

    void printTree() {
        printTree(root, 0);
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

    sval_t bst_find(const int tid, skey_t key);

    sval_t bst_insert(const int tid, skey_t key, sval_t value);

    sval_t bst_delete(const int tid, skey_t key);

    node_t<skey_t, sval_t> *get_root() {
        return root;
    }

    RecMgr *debugGetRecMgr() {
        return recmgr;
    }
};


#endif /* ELLEN_H */

