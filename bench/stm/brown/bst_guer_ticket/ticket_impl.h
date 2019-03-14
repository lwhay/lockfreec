/*   
 *   File: bst_tk.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: Asynchronized Concurrency: The Secret to Scaling Concurrent
 *    Search Data Structures, Tudor David, Rachid Guerraoui, Vasileios Trigonakis,
 *   ASPLOS '15
 *   bst_tk.c is part of ASCYLIB
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
 * File:   ticket_impl.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 7, 2017, 1:38 PM
 */

#ifndef TICKET_IMPL_H
#define TICKET_IMPL_H

#include "ticket.h"

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *
ticket<skey_t, sval_t, RecMgr>::new_node(const int tid, skey_t key, sval_t val, node_t<skey_t, sval_t> *l,
                                         node_t<skey_t, sval_t> *r) {
    auto node = new_node_no_init(tid);
    node->val = val;
    node->key = key;
    node->left = l;
    node->right = r;
    return node;
}

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ticket<skey_t, sval_t, RecMgr>::new_node_no_init(const int tid) {
    auto node = recmgr->template allocate<node_t<skey_t, sval_t>>(tid);
    if (unlikely(node == NULL)) {
        perror("malloc @ new_node");
        exit(1);
    }
    node->lock.to_uint64 = 0;
    node->val = NO_VALUE;
    return node;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ticket<skey_t, sval_t, RecMgr>::bst_tk_find(const int tid, skey_t key) {
    auto guard = recmgr->getGuard(tid, true);
    node_t<skey_t, sval_t> *curr = root;

    while (likely(curr->left != NULL)) {
        if (key < curr->key) {
            curr = curr->left;
        } else {
            curr = curr->right;
        }
    }

    if (curr->key == key) {
        return curr->val;
    }

    return NO_VALUE;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ticket<skey_t, sval_t, RecMgr>::bst_tk_insert(const int tid, skey_t key, sval_t val) {
    node_t<skey_t, sval_t> *curr;
    node_t<skey_t, sval_t> *pred = NULL;
    volatile uint64_t curr_ver = 0;
    uint64_t pred_ver = 0, right = 0;

    retry:
    { // reclamation guarded section
        auto guard = recmgr->getGuard(tid);
        curr = root;
        do {
            curr_ver = curr->lock.to_uint64;

            pred = curr;
            pred_ver = curr_ver;

            if (key < curr->key) {
                right = 0;
                curr = curr->left;
            } else {
                right = 1;
                curr = curr->right;
            }
        } while (likely(curr->left != NULL));

        if (curr->key == key) {
            // insert if absent
            return curr->val;
        }

//        node_t<skey_t, sval_t>* nn_leaked = new_node(tid, key, val, NULL, NULL);
        node_t<skey_t, sval_t> *nn = new_node(tid, key, val, NULL, NULL);
        node_t<skey_t, sval_t> *nr = new_node_no_init(tid);

        if ((!tl_trylock_version(&pred->lock, (volatile tl_t *) &pred_ver, right))) {
            recmgr->deallocate(tid, nn);
            recmgr->deallocate(tid, nr);
            goto retry;
        }

        if (key < curr->key) {
            nr->key = curr->key;
            nr->left = nn;
            nr->right = curr;
        } else {
            nr->key = key;
            nr->left = curr;
            nr->right = nn;
        }

        if (right) {
            pred->right = nr;
        } else {
            pred->left = nr;
        }

        tl_unlock(&pred->lock, right);

        return NO_VALUE;
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ticket<skey_t, sval_t, RecMgr>::bst_tk_delete(const int tid, skey_t key) {
    node_t<skey_t, sval_t> *curr;
    node_t<skey_t, sval_t> *pred = NULL;
    node_t<skey_t, sval_t> *ppred = NULL;
    volatile uint64_t curr_ver = 0;
    uint64_t pred_ver = 0, ppred_ver = 0, right = 0, pright = 0;

    retry:

    { // reclamation guarded section
        auto guard = recmgr->getGuard(tid);
        curr = root;

        do {
            curr_ver = curr->lock.to_uint64;

            ppred = pred;
            ppred_ver = pred_ver;
            pright = right;

            pred = curr;
            pred_ver = curr_ver;

            if (key < curr->key) {
                right = 0;
                curr = curr->left;
            } else {
                right = 1;
                curr = curr->right;
            }
        } while (likely(curr->left != NULL));

        if (curr->key != key) {
            return NO_VALUE;
        }

        if ((!tl_trylock_version(&ppred->lock, (volatile tl_t *) &ppred_ver, pright))) {
            goto retry;
        }

        if ((!tl_trylock_version_both(&pred->lock, (volatile tl_t *) &pred_ver))) {
            tl_revert(&ppred->lock, pright);
            goto retry;
        }

        if (pright) {
            if (right) {
                ppred->right = pred->left;
            } else {
                ppred->right = pred->right;
            }

        } else {
            if (right) {
                ppred->left = pred->left;
            } else {
                ppred->left = pred->right;
            }
        }

        tl_unlock(&ppred->lock, pright);

        recmgr->retire(tid, curr);
        recmgr->retire(tid, pred);

        return curr->val;
    }
}

#endif /* TICKET_IMPL_H */

