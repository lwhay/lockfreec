/**
 * Implementation of the lock-free external BST of Natarajan and Mittal.
 * Trevor Brown, 2017. (Based on Natarajan's original code.)
 * 
 * I made several changes/fixes to the data structure,
 * and there are four different versions: baseline, stage 0, stage 1, stage 1.
 * 
 * Baseline is functionally the same as the original implementation by Natarajan
 * (but converted to a class, and with templates/generics).
 *
 * Stage 0:
 *  - Fixed a concurrency bug (missing volatiles)
 * 
 * Delta from stage 0 to stage 1:
 *  - Added proper node allocation
 *    (The original implementation explicitly allocated arrays of 2 nodes at a time,
 *     which effectively prevents any real memory reclamation.
 *     [You can't free the nodes individually. Rather, you must free the arrays,
 *      and only after BOTH nodes are safe to free. This is hard to solve.])
 * 
 * Delta from stage 1 to stage 2:
 *  - Added proper memory reclamation
 *    (The original implementation leaked all memory.
 *     The implementation in ASCYLIB tried to reclaim memory (using epoch
 *     based reclamation?), but still leaked a huge amount of memory.
 *     It turns out the original algorithm lacks a necessary explanation of
 *     how you should reclaim memory in a system without garbage collection.
 *     Because of the way deletions are aggregated and performed at once,
 *     after you delete a single key, you may have to reclaim many nodes.)
 *    To my knowledge, this is the only correct, existing implementation
 *    of this algorithm (as of Mar 2018).
 * 
 * To compile baseline, add compilation arguments:
 *  -DDS_H_FILE=ds/natarajan_ext_bst_lf/natarajan_ext_bst_lf_baseline_impl.h
 *  -DBASELINE
 * 
 * To compile stage 0, add compilation argument:
 *  -DDS_H_FILE=ds/natarajan_ext_bst_lf/natarajan_ext_bst_lf_baseline_impl.h
 * 
 * To compile stage 1, add compilation argument:
 *  -DDS_H_FILE=ds/natarajan_ext_bst_lf/natarajan_ext_bst_lf_stage1_impl.h
 * 
 * To compile stage 2, add compilation argument:
 *  -DDS_H_FILE=ds/natarajan_ext_bst_lf/natarajan_ext_bst_lf_stage2_impl.h
 */

/*A Lock Free Binary Search Tree
 * File:
 *   wfrbt.cpp
 * Author(s):
 *   Aravind Natarajan <natarajan.aravind@gmail.com>
 * Description:
 *   A Lock Free Binary Search Tree
 *
 * Copyright (c) 20013-2014.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

Please cite our PPoPP 2014 paper - Fast Concurrent Lock-Free Binary Search Trees by Aravind Natarajan and Neeraj Mittal if you use our code in your experiments	

Features:
1. Insert operations directly install their window without injecting the operation into the tree. They help any conflicting operation at the injection point, 
before executing their window txn.
2. Delete operations are the same as that of the original algorithm.
 
 */

/* 
 * File:   wfrbt_impl.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 8, 2017, 10:45 AM
 */

#ifndef NATARAJAN_EXT_BST_LF_IMPL_H
#define NATARAJAN_EXT_BST_LF_IMPL_H

#include "natarajan_ext_bst_lf_baseline.h"

static inline bool SetBit(volatile size_t *array, int bit) {
    bool flag;
    __asm__ __volatile__("lock bts %2,%1; setb %0" : "=q" (flag) : "m" (*array), "r" (bit));
    return flag;
}

static bool mark_Node(volatile AO_t *word) {
    return (SetBit(word, MARK_BIT));
}

static volatile AO_t stop = 0;
static volatile AO_t stop2 = 0;

//long total_insert = 0;

/* STRUCTURES */
enum {
    Front, Back
};

//long blackCount = -1;
//long leafNodes = 0;

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
seekRecord_t<skey_t, sval_t> *
natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::insseek(thread_data_t<skey_t, sval_t> *data, skey_t key,
                                                               int op) {

    node_t<skey_t, sval_t> *gpar = NULL; // last node (ancestor of parent on access path) whose child pointer field is unmarked
    node_t<skey_t, sval_t> *par = data->rootOfTree;
    node_t<skey_t, sval_t> *leaf;
    node_t<skey_t, sval_t> *leafchild;


    AO_t parentPointerWord = (size_t) NULL; // contents in gpar
    AO_t leafPointerWord = par->child.AO_val1; // contents in par. Tree has two imaginary keys \inf_{1} and \inf_{2} which are larger than all other keys. 
    AO_t leafchildPointerWord; // contents in leaf

    bool isparLC = false; // is par the left child of gpar
    bool isleafLC = true; // is leaf the left child of par
    bool isleafchildLC; // is leafchild the left child of leaf


    leaf = (node_t<skey_t, sval_t> *) get_addr(leafPointerWord);
    if (cmp(key, leaf->key)) {
        leafchildPointerWord = leaf->child.AO_val1;
        isleafchildLC = true;

    } else {
        leafchildPointerWord = leaf->child.AO_val2;
        isleafchildLC = false;
    }

    leafchild = (node_t<skey_t, sval_t> *) get_addr(leafchildPointerWord);


    while (leafchild != NULL) {


        if (!is_marked(leafPointerWord)) {
            gpar = par;
            parentPointerWord = leafPointerWord;
            isparLC = isleafLC;
        }

        par = leaf;
        leafPointerWord = leafchildPointerWord;
        isleafLC = isleafchildLC;

        leaf = leafchild;


        if (cmp(key, leaf->key)) {
            leafchildPointerWord = leaf->child.AO_val1;
            isleafchildLC = true;
        } else {
            leafchildPointerWord = leaf->child.AO_val2;
            isleafchildLC = false;
        }

        leafchild = (node_t<skey_t, sval_t> *) get_addr(leafchildPointerWord);

    }

//    if (key == leaf->key) {
//        // key matches that being inserted	
//        return NULL;
//    }

    seekRecord_t<skey_t, sval_t> *R = data->sr;
    R->leafKey = leaf->key;
#ifndef BASELINE
    R->leafValue = leaf->value;
#endif
    R->parent = par;
    R->pL = leafPointerWord;
    R->isLeftL = isleafLC;
    R->lum = gpar;
    R->lumC = parentPointerWord;
    R->isLeftUM = isparLC;
    return R;
}

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
seekRecord_t<skey_t, sval_t> *
natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::delseek(thread_data_t<skey_t, sval_t> *data, skey_t key,
                                                               int op) {
    node_t<skey_t, sval_t> *gpar = NULL; // last node (ancestor of parent on access path) whose child pointer field is unmarked
    node_t<skey_t, sval_t> *par = data->rootOfTree;
    node_t<skey_t, sval_t> *leaf;
    node_t<skey_t, sval_t> *leafchild;


    AO_t parentPointerWord = (AO_t) NULL; // contents in gpar
    AO_t leafPointerWord = par->child.AO_val1; // contents in par. Tree has two imaginary keys \inf_{1} and \inf_{2} which are larger than all other keys. 
    AO_t leafchildPointerWord; // contents in leaf

    bool isparLC = false; // is par the left child of gpar
    bool isleafLC = true; // is leaf the left child of par
    bool isleafchildLC; // is leafchild the left child of leaf


    leaf = (node_t<skey_t, sval_t> *) get_addr(leafPointerWord);
    if (cmp(key, leaf->key)) {
        leafchildPointerWord = leaf->child.AO_val1;
        isleafchildLC = true;

    } else {
        leafchildPointerWord = leaf->child.AO_val2;
        isleafchildLC = false;
    }

    leafchild = (node_t<skey_t, sval_t> *) get_addr(leafchildPointerWord);


    while (leafchild != NULL) {


        if (!is_marked(leafPointerWord)) {
            gpar = par;
            parentPointerWord = leafPointerWord;
            isparLC = isleafLC;
        }

        par = leaf;
        leafPointerWord = leafchildPointerWord;
        isleafLC = isleafchildLC;

        leaf = leafchild;


        if (cmp(key, leaf->key)) {
            leafchildPointerWord = leaf->child.AO_val1;
            isleafchildLC = true;
        } else {
            leafchildPointerWord = leaf->child.AO_val2;
            isleafchildLC = false;
        }

        leafchild = (node_t<skey_t, sval_t> *) get_addr(leafchildPointerWord);

    }

    // op = DELETE
    if (key != leaf->key) {
        // key is not found in the tree.
        return NULL;
    }

    seekRecord_t<skey_t, sval_t> *R = data->sr;
    R->leafKey = leaf->key;
#ifndef BASELINE
    R->leafValue = leaf->value;
#endif
    R->parent = par;
    R->pL = leafPointerWord;
    R->isLeftL = isleafLC;
    R->lum = gpar;
    R->lumC = parentPointerWord;
    R->isLeftUM = isparLC;


    return R;
}

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
seekRecord_t<skey_t, sval_t> *
natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::secondary_seek(thread_data_t<skey_t, sval_t> *data, skey_t key,
                                                                      seekRecord_t<skey_t, sval_t> *sr) {

    //std::cout << "sseek" << std::endl;
    node_t<skey_t, sval_t> *flaggedLeaf = (node_t<skey_t, sval_t> *) get_addr(sr->pL);
    node_t<skey_t, sval_t> *gpar = NULL; // last node (ancestor of parent on access path) whose child pointer field is unmarked
    node_t<skey_t, sval_t> *par = data->rootOfTree;
    node_t<skey_t, sval_t> *leaf;
    node_t<skey_t, sval_t> *leafchild;


    AO_t parentPointerWord = (AO_t) NULL; // contents in gpar
    AO_t leafPointerWord = par->child.AO_val1; // contents in par. Tree has two imaginary keys \inf_{1} and \inf_{2} which are larger than all other keys. 
    AO_t leafchildPointerWord; // contents in leaf

    bool isparLC = false; // is par the left child of gpar
    bool isleafLC = true; // is leaf the left child of par
    bool isleafchildLC; // is leafchild the left child of leaf


    leaf = (node_t<skey_t, sval_t> *) get_addr(leafPointerWord);
    if (cmp(key, leaf->key)) {
        leafchildPointerWord = leaf->child.AO_val1;
        isleafchildLC = true;

    } else {
        leafchildPointerWord = leaf->child.AO_val2;
        isleafchildLC = false;
    }

    leafchild = (node_t<skey_t, sval_t> *) get_addr(leafchildPointerWord);


    while (leafchild != NULL) {


        if (!is_marked(leafPointerWord)) {
            gpar = par;
            parentPointerWord = leafPointerWord;
            isparLC = isleafLC;
        }

        par = leaf;
        leafPointerWord = leafchildPointerWord;
        isleafLC = isleafchildLC;

        leaf = leafchild;


        if (cmp(key, leaf->key)) {
            leafchildPointerWord = leaf->child.AO_val1;
            isleafchildLC = true;
        } else {
            leafchildPointerWord = leaf->child.AO_val2;
            isleafchildLC = false;
        }

        leafchild = (node_t<skey_t, sval_t> *) get_addr(leafchildPointerWord);

    }


    if (!is_flagged(leafPointerWord) || (leaf != flaggedLeaf)) {
        // operation has been completed by another process.
        return NULL;
    }

    seekRecord_t<skey_t, sval_t> *R = data->ssr;

    R->leafKey = leaf->key;
    R->parent = par;
    R->pL = leafPointerWord;
    R->isLeftL = isleafLC;
    R->lum = gpar;
    R->lumC = parentPointerWord;
    R->isLeftUM = isparLC;
    return R;
}

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
sval_t natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::search(thread_data_t<skey_t, sval_t> *data, skey_t key) {
    node_t<skey_t, sval_t> *cur = (node_t<skey_t, sval_t> *) get_addr(data->rootOfTree->child.AO_val1);
    skey_t lastKey = 0;
    node_t<skey_t, sval_t> *lastNode = NULL;
    while (cur != NULL) {
        lastKey = cur->key;
        lastNode = cur;
        cur = (cmp(key, lastKey) ? (node_t<skey_t, sval_t> *) get_addr(cur->child.AO_val1)
                                 : (node_t<skey_t, sval_t> *) get_addr(cur->child.AO_val2));
    }
    if (key == lastKey) {
#ifdef BASELINE
        return (sval_t) key;
#else
        return lastNode->value;
#endif

    }
    return NO_VALUE;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------------------

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
int
natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::help_conflicting_operation(thread_data_t<skey_t, sval_t> *data,
                                                                                  seekRecord_t<skey_t, sval_t> *R) {
    if (is_flagged(R->pL)) {
        // leaf node is flagged for deletion by another process.

        //1. mark sibling of leaf node for deletion and then read its contents.

        AO_t pS;

        if (R->isLeftL) {
            // L is the left child of P
            mark_Node(&R->parent->child.AO_val2);
            pS = R->parent->child.AO_val2;

        } else {
            mark_Node(&R->parent->child.AO_val1);
            pS = R->parent->child.AO_val1;
        }

        // 2. Execute cas on the last unmarked node to remove the 
        // if pS is flagged, propagate it. 
        AO_t newWord;

        if (is_flagged(pS)) {
            newWord = create_child_word((node_t<skey_t, sval_t> *) get_addr(pS), UNMARK, FLAG);
        } else {
            newWord = create_child_word((node_t<skey_t, sval_t> *) get_addr(pS), UNMARK, UNFLAG);
        }

        int result;

        assert(is_free(R->lumC));
        if (R->isLeftUM) {
            result = atomic_cas_full(&R->lum->child.AO_val1, R->lumC, newWord);
        } else {
            result = atomic_cas_full(&R->lum->child.AO_val2, R->lumC, newWord);
        }

        return result;
    } else {
        // leaf node is marked for deletion by another process.
        // Note that leaf is not flagged, as it will be taken care of in the above case.

        AO_t newWord;

        if (is_flagged(R->pL)) {
            newWord = create_child_word((node_t<skey_t, sval_t> *) get_addr(R->pL), UNMARK, FLAG);
        } else {
            newWord = create_child_word((node_t<skey_t, sval_t> *) get_addr(R->pL), UNMARK, UNFLAG);
        }

        int result;

        if (R->isLeftUM) {
            result = atomic_cas_full(&R->lum->child.AO_val1, R->lumC, newWord);
        } else {
            result = atomic_cas_full(&R->lum->child.AO_val2, R->lumC, newWord);
        }

        return result;
    }

}

//-------------------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------------------

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
int natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::inject(thread_data_t<skey_t, sval_t> *data,
                                                                  seekRecord_t<skey_t, sval_t> *R, int op) {

    // pL is free		
    //1. Flag L		
    assert(is_free(R->pL));
    AO_t newWord = create_child_word((node_t<skey_t, sval_t> *) get_addr(R->pL), UNMARK, FLAG);
    int result;
    if (R->isLeftL) {
        result = atomic_cas_full(&R->parent->child.AO_val1, R->pL, newWord);

    } else {
        result = atomic_cas_full(&R->parent->child.AO_val2, R->pL, newWord);
    }
    return result;
}

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
sval_t
natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::insertIfAbsent(thread_data_t<skey_t, sval_t> *data, skey_t key,
                                                                      sval_t value) {
    int injectResult;
    while (true) {
        seekRecord_t<skey_t, sval_t> *R = insseek(data, key, INSERT);
        if (R->leafKey == key) {
#ifdef BASELINE
            return (sval_t) key;
#else
            return R->leafValue;
#endif
        }
        if (!is_free(R->pL)) {
            help_conflicting_operation(data, R);
            continue;
        }
        // key not present in the tree. Insert		
        injectResult = perform_one_insert_window_operation(data, R, key, value);
        if (injectResult == 1) {
            // Operation injected and executed			
            return NO_VALUE;
        }
    }
    // execute insert window operation.		
}

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
sval_t
natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::delete_node(thread_data_t<skey_t, sval_t> *data, skey_t key) {
    while (true) {
        seekRecord_t<skey_t, sval_t> *R = delseek(data, key, DELETE);
        if (R == NULL) {
            return NO_VALUE;
        }
        // key is present in the tree. Inject operation into the tree		
        if (!is_free(R->pL)) {
            help_conflicting_operation(data, R);
            continue;
        }
        int injectResult = inject(data, R, DELETE);
        if (injectResult == 1) {
#ifdef BASELINE
            sval_t retval = (sval_t) key;
#else
            sval_t retval = R->leafValue;
#endif
            // Operation injected 			
            //data->numActualDelete++;			
            int res = perform_one_delete_window_operation(data, R, key);
            if (res == 1) {
                // operation successfully executed.
                return retval;
            } else {
                // window transaction could not be executed.
                // perform secondary seek.				
                while (true) {
                    R = secondary_seek(data, key, R);
                    if (R == NULL) {
                        // flagged leaf not found. Operation has been executed by some other process.
                        return retval;
                    }
                    res = perform_one_delete_window_operation(data, R, key);
                    if (res == 1) {
                        return retval;
                    }
                }
            }
        }
        // otherwise, operation was not injected. Restart.
    }
}

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
int natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::perform_one_insert_window_operation(
        thread_data_t<skey_t, sval_t> *data, seekRecord_t<skey_t, sval_t> *R, skey_t newKey, sval_t value) {
    node_t<skey_t, sval_t> *newInt;
    node_t<skey_t, sval_t> *newLeaf;
    //		if(data->recycledNodes.empty()){		
    node_t<skey_t, sval_t> *allocedNodeArr = (node_t<skey_t, sval_t> *) malloc(
            2 * sizeof(struct node_t<skey_t, sval_t>)); // new pointerNode_t[2];
    newInt = &allocedNodeArr[0];
    newLeaf = &allocedNodeArr[1];
#ifdef GSTATS_HANDLE_STATS
    GSTATS_APPEND(data->id, node_allocated_addresses, (long long) newInt);
#endif
#ifdef GSTATS_HANDLE_STATS
    GSTATS_APPEND(data->id, node_allocated_addresses, (long long) newLeaf);
#endif

    /*		}	
                    else{
                            // reuse memory of previously allocated nodes.
                            newInt = data->recycledNodes.back();
                            data->recycledNodes.pop_back();			
                            newLeaf = data->recycledNodes.back();
                            data->recycledNodes.pop_back();
                    }
     */
    newLeaf->child.AO_val1 = (size_t) NULL;
    newLeaf->child.AO_val2 = (size_t) NULL;
    newLeaf->key = newKey;
#ifndef BASELINE
    newLeaf->value = value;
#endif
    assert(is_free(R->pL));
    node_t<skey_t, sval_t> *existLeaf = (node_t<skey_t, sval_t> *) get_addr(R->pL);

    skey_t existKey = R->leafKey;


    if (cmp(newKey, existKey)) {
        // key is to be inserted on lchild
        newInt->key = existKey;
        newInt->child.AO_val1 = create_child_word(newLeaf, 0, 0);
        newInt->child.AO_val2 = create_child_word(existLeaf, 0, 0);

    } else {
        // key is to be inserted on rchild
        newInt->key = newKey;
        newInt->child.AO_val2 = create_child_word(newLeaf, 0, 0);
        newInt->child.AO_val1 = create_child_word(existLeaf, 0, 0);

    }

    // cas to replace window		
    AO_t newCasField;
    newCasField = create_child_word(newInt, UNMARK, UNFLAG);
    int result;

    if (R->isLeftL) {
        result = atomic_cas_full(&R->parent->child.AO_val1, R->pL, newCasField);
    } else {
        result = atomic_cas_full(&R->parent->child.AO_val2, R->pL, newCasField);
    }
    if (result == 1) {
        // successfully inserted.			
        //data->numInsert++;
        return 1;
    } else {
        // reuse data and pointer nodes				
        //data->recycledNodes.push_back(newInt);
        //data->recycledNodes.push_back(newLeaf);
        return 0;

    }

}

/*************************************************************************************************/

template<typename skey_t, typename sval_t, class RecMgr, class Compare>
int natarajan_ext_bst_lf<skey_t, sval_t, RecMgr, Compare>::perform_one_delete_window_operation(
        thread_data_t<skey_t, sval_t> *data, seekRecord_t<skey_t, sval_t> *R, skey_t key) {
    // mark sibling.
    AO_t pS;
    if (R->isLeftL) {
        // L is the left child of P
        mark_Node(&R->parent->child.AO_val2);
        pS = R->parent->child.AO_val2;

    } else {
        mark_Node(&R->parent->child.AO_val1);
        pS = R->parent->child.AO_val1;
    }

    AO_t newWord;

    if (is_flagged(pS)) {
        newWord = create_child_word((node_t<skey_t, sval_t> *) get_addr(pS), UNMARK, FLAG);
    } else {
        newWord = create_child_word((node_t<skey_t, sval_t> *) get_addr(pS), UNMARK, UNFLAG);
    }

    int result;

    if (R->isLeftUM) {
        result = atomic_cas_full(&R->lum->child.AO_val1, R->lumC, newWord);
    } else {
        result = atomic_cas_full(&R->lum->child.AO_val2, R->lumC, newWord);
    }
    return result;
}

#endif /* NATARAJAN_EXT_BST_LF_IMPL_H */

