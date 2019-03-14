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
 * File:   howley_impl.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 1, 2017, 12:52 PM
 */
#pragma once

#include "howley.h"

const unsigned int val_mask = ~(0x3);

template<typename skey_t, typename sval_t, class RecMgr>
sval_t howley<skey_t, sval_t, RecMgr>::bst_contains(const int tid, skey_t k) {
    node_t<skey_t, sval_t> *pred, *curr;
    operation_t<skey_t, sval_t> *pred_op, *curr_op;
    auto res = bst_find(tid, k, &pred, &pred_op, &curr, &curr_op, root, root);
    if (res.code == FOUND) return res.val;
    return NO_VALUE;
}

template<typename skey_t, typename sval_t, class RecMgr>
find_result<sval_t> howley<skey_t, sval_t, RecMgr>::bst_find(const int tid, skey_t k, node_t<skey_t, sval_t> **pred,
                                                             operation_t<skey_t, sval_t> **pred_op,
                                                             node_t<skey_t, sval_t> **curr,
                                                             operation_t<skey_t, sval_t> **curr_op,
                                                             node_t<skey_t, sval_t> *aux_root,
                                                             node_t<skey_t, sval_t> *root) {
    find_result<sval_t> result;
    skey_t curr_key;
    node_t<skey_t, sval_t> *next;
    node_t<skey_t, sval_t> *last_right;
    operation_t<skey_t, sval_t> *last_right_op;
    retry:
    auto guard = recmgr->getGuard(tid, true);

    result.val = NO_VALUE;
    result.code = NOT_FOUND_R;
    *curr = aux_root;
    *curr_op = (*curr)->op;
    if (GETFLAG(*curr_op) != STATE_OP_NONE) {
        if (aux_root == root) {
            bst_help_child_cas(tid, UNFLAG(*curr_op), *curr);
            goto retry;
        } else {
            result.code = ABORT;
            return result;
        }
    }
    next = (*curr)->right;
    last_right = *curr;
    last_right_op = *curr_op;
    while (!ISNULL(next)) {
        *pred = *curr;
        *pred_op = *curr_op;
        *curr = next;
        *curr_op = (*curr)->op;

        if (GETFLAG(*curr_op) != STATE_OP_NONE) {
            bst_help(tid, *pred, *pred_op, *curr, *curr_op);
            goto retry;
        }
        curr_key = (*curr)->key;
        if (k < curr_key) {
            result.code = NOT_FOUND_L;
            next = (*curr)->left;
        } else if (k > curr_key) {
            result.code = NOT_FOUND_R;
            next = (*curr)->right;
            last_right = *curr;
            last_right_op = *curr_op;
        } else {
            result.val = (*curr)->value;
            result.code = FOUND;
            break;
        }
    }
    if ((!(result.code == FOUND)) && (last_right_op != last_right->op)) goto retry;
    if ((*curr)->op != *curr_op) goto retry;
    return result;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t howley<skey_t, sval_t, RecMgr>::bst_add(const int tid, skey_t k, sval_t v) {
    node_t<skey_t, sval_t> *pred;
    node_t<skey_t, sval_t> *curr;
    operation_t<skey_t, sval_t> *pred_op;
    operation_t<skey_t, sval_t> *curr_op;
    while (true) {
        auto guard = recmgr->getGuard(tid);

        auto result = bst_find(tid, k, &pred, &pred_op, &curr, &curr_op, root, root);
        if (result.code == FOUND) return result.val;
        bool is_left = (result.code == NOT_FOUND_L);
        auto new_node = create_node(tid, k, v, NULL_NODEPTR, NULL_NODEPTR);
        auto old = is_left ? curr->left : curr->right;
        auto cas_op = alloc_op(tid);
        cas_op->child_cas_op.is_left = is_left;
        cas_op->child_cas_op.expected = old;
        cas_op->child_cas_op.update = new_node;

        if (CASB(&curr->op, curr_op, FLAG(cas_op, STATE_OP_CHILDCAS))) {
            bst_help_child_cas(tid, cas_op, curr);
            return NO_VALUE;
        } else {
            recmgr->deallocate(tid, new_node);
            recmgr->deallocate(tid, cas_op);
        }
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t howley<skey_t, sval_t, RecMgr>::bst_remove(const int tid, skey_t k) {
    node_t<skey_t, sval_t> *pred;
    node_t<skey_t, sval_t> *curr;
    node_t<skey_t, sval_t> *replace;
    operation_t<skey_t, sval_t> *pred_op;
    operation_t<skey_t, sval_t> *curr_op;
    operation_t<skey_t, sval_t> *replace_op;
    while (true) {
        auto guard = recmgr->getGuard(tid);

        auto result = bst_find(tid, k, &pred, &pred_op, &curr, &curr_op, root, root);
        if (!(result.code == FOUND)) return NO_VALUE;
        if (ISNULL(curr->right) || ISNULL(curr->left)) {
            if (CASB(&curr->op, curr_op, FLAG(curr_op, STATE_OP_MARK))) {
                bst_help_marked(tid, pred, pred_op, curr);
                recmgr->retire(tid, curr);
                return result.val;
            }
        } else {
            auto result2 = bst_find(tid, k, &pred, &pred_op, &replace, &replace_op, curr, root);
            if ((result2.code == ABORT) || (curr->op != curr_op)) continue;
            auto reloc_op = alloc_op(tid);
            reloc_op->relocate_op.state = STATE_OP_ONGOING;
            reloc_op->relocate_op.dest = curr;
            reloc_op->relocate_op.dest_op = curr_op;
            reloc_op->relocate_op.remove_key = k;
            reloc_op->relocate_op.remove_value = result.val;
            reloc_op->relocate_op.replace_key = replace->key;
            reloc_op->relocate_op.replace_value = replace->value;

            if (CASB(&replace->op, replace_op, FLAG(reloc_op, STATE_OP_RELOCATE))) {
                if (bst_help_relocate(tid, reloc_op, pred, pred_op, replace)) {
                    return result.val;
                }
                recmgr->retire(tid, reloc_op);
            } else {
                recmgr->deallocate(tid, reloc_op);
            }
        }
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
bool howley<skey_t, sval_t, RecMgr>::bst_help_relocate(const int tid, operation_t<skey_t, sval_t> *op,
                                                       node_t<skey_t, sval_t> *pred,
                                                       operation_t<skey_t, sval_t> *pred_op,
                                                       node_t<skey_t, sval_t> *curr) {
    int seen_state = op->relocate_op.state;
    if (seen_state == STATE_OP_ONGOING) {
        auto seen_op = CASV(&op->relocate_op.dest->op, op->relocate_op.dest_op, FLAG(op, STATE_OP_RELOCATE));
        if ((seen_op == op->relocate_op.dest_op) || (seen_op == FLAG(op, STATE_OP_RELOCATE))) {
            CASV(&op->relocate_op.state, STATE_OP_ONGOING, STATE_OP_SUCCESSFUL);
            seen_state = STATE_OP_SUCCESSFUL;
        } else {
            seen_state = CASV(&op->relocate_op.state, STATE_OP_ONGOING, STATE_OP_FAILED);
        }
    }
    if (seen_state == STATE_OP_SUCCESSFUL) {
        CASB(&op->relocate_op.dest->key, op->relocate_op.remove_key, op->relocate_op.replace_key);
        CASB(&op->relocate_op.dest->value, op->relocate_op.remove_value, op->relocate_op.replace_value);
        CASB(&op->relocate_op.dest->op, FLAG(op, STATE_OP_RELOCATE), FLAG(op, STATE_OP_NONE));
    }
    bool result = (seen_state == STATE_OP_SUCCESSFUL);
    if (op->relocate_op.dest == curr) return result;
    CASB(&curr->op, FLAG(op, STATE_OP_RELOCATE), FLAG(op, result ? STATE_OP_MARK : STATE_OP_NONE));
    if (result) {
        if (op->relocate_op.dest == pred) pred_op = FLAG(op, STATE_OP_NONE);
        bst_help_marked(tid, pred, pred_op, curr);
    }
    return result;
}

template<typename skey_t, typename sval_t, class RecMgr>
void howley<skey_t, sval_t, RecMgr>::bst_help_child_cas(const int tid, operation_t<skey_t, sval_t> *op,
                                                        node_t<skey_t, sval_t> *dest) {
    auto address = (op->child_cas_op.is_left) ? &dest->left : &dest->right;
    CASB(address, op->child_cas_op.expected, op->child_cas_op.update);
    if (CASB(&dest->op, FLAG(op, STATE_OP_CHILDCAS), FLAG(op, STATE_OP_NONE))) {
        recmgr->retire(tid, op);
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
void howley<skey_t, sval_t, RecMgr>::bst_help_marked(const int tid, node_t<skey_t, sval_t> *pred,
                                                     operation_t<skey_t, sval_t> *pred_op,
                                                     node_t<skey_t, sval_t> *curr) {
    auto new_ref = ISNULL(curr->left) ? (ISNULL(curr->right) ? SETNULL(curr) : curr->right) : curr->left;
    auto cas_op = alloc_op(tid);
    cas_op->child_cas_op.is_left = (curr == pred->left);
    cas_op->child_cas_op.expected = curr;
    cas_op->child_cas_op.update = new_ref;
    if (CASB(&pred->op, pred_op, FLAG(cas_op, STATE_OP_CHILDCAS))) {
        bst_help_child_cas(tid, cas_op, pred);
    } else {
        recmgr->deallocate(tid, cas_op);
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
void howley<skey_t, sval_t, RecMgr>::bst_help(const int tid, node_t<skey_t, sval_t> *pred,
                                              operation_t<skey_t, sval_t> *pred_op, node_t<skey_t, sval_t> *curr,
                                              operation_t<skey_t, sval_t> *curr_op) {
    if (GETFLAG(curr_op) == STATE_OP_CHILDCAS) {
        bst_help_child_cas(tid, UNFLAG(curr_op), curr);
    } else if (GETFLAG(curr_op) == STATE_OP_RELOCATE) {
        bst_help_relocate(tid, UNFLAG(curr_op), pred, pred_op, curr);
    } else if (GETFLAG(curr_op) == STATE_OP_MARK) {
        bst_help_marked(tid, pred, pred_op, curr);
    }
}
