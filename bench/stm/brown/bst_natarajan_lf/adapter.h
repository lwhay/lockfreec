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

#ifndef NATARAJAN_EXT_BST_LF_ADAPTER_H
#define NATARAJAN_EXT_BST_LF_ADAPTER_H


#include <iostream>
#include "errors.h"
#include "random_fnv1a.h"

#ifdef DS_H_FILE
#include STR(DS_H_FILE)
#else
//    #warning Using default data structure implementation (see define DS_H_FILE)
//    #include "natarajan_ext_bst_lf_baseline_impl.h"
#include "natarajan_ext_bst_lf_stage2_impl.h"

#endif
#ifdef USE_TREE_STATS
#   include "tree_stats.h"
#endif

#define RECORD_MANAGER_T record_manager<Reclaim, Alloc, Pool, node_t<K, V>>
#define DATA_STRUCTURE_T natarajan_ext_bst_lf<K, V, RECORD_MANAGER_T>

template<typename K, typename V, class Reclaim = reclaimer_debra<K>, class Alloc = allocator_new<K>, class Pool = pool_none<K>>
class ds_adapter {
private:
    const V NO_VALUE;
    DATA_STRUCTURE_T *const tree;

public:
    ds_adapter(const int NUM_THREADS,
               const K &unused1,
               const K &KEY_POS_INFTY,
               const V &VALUE_RESERVED,
               RandomFNV1A *const unused2)
            : NO_VALUE(VALUE_RESERVED), tree(new DATA_STRUCTURE_T(KEY_POS_INFTY, NO_VALUE, NUM_THREADS)) {}

    ~ds_adapter() {
        delete tree;
    }

    V getNoValue() {
        return NO_VALUE;
    }

    void initThread(const int tid) {
        tree->initThread(tid);
    }

    void deinitThread(const int tid) {
        tree->deinitThread(tid);
    }

    bool contains(const int tid, const K &key) {
        return tree->find(tid, key) != getNoValue();
    }

    V insert(const int tid, const K &key, const V &val) {
        setbench_error("insert-replace not implemented for this data structure");
    }

    V insertIfAbsent(const int tid, const K &key, const V &val) {
        return tree->insertIfAbsent(tid, key, val);
    }

    V erase(const int tid, const K &key) {
        //const std::std::pair<V,bool> erase(const int tid, const K& key) {
        //V retval = tree->erase(tid, key);
        //return std::std::pair<V,bool>(retval, retval != getNoValue());
        return tree->erase(tid, key);
    }

    V find(const int tid,
           const K &key) {                                  //////////// TODO: USE FIND WITH VALUE RETURN TYPE FOR DBX!!!!
//    //const std::std::pair<V,bool> find(const int tid, const K& key) {
//        //V retval = tree->find(tid, key);
//        //return std::std::pair<V,bool>(retval, retval != getNoValue());
        return tree->find(tid, key);
    }

    int rangeQuery(const int tid, const K &lo, const K &hi, K *const resultKeys, V *const resultValues) {
        setbench_error("rangeQuery not implemented for this data structure");
    }

    void printSummary() {
        tree->printSummary();
    }

    bool validateStructure() {
        return tree->validateStructure();
    }

    void printObjectSizes() {
        std::cout << "sizes: node=" << (sizeof(node_t<K, V>)) << std::endl;
    }

#ifdef USE_TREE_STATS
    class NodeHandler {
    public:
        typedef node_t<K,V> * NodePtrType;
        K minKey;
        K maxKey;
        
        NodeHandler(const K& _minKey, const K& _maxKey) {
            minKey = _minKey;
            maxKey = _maxKey;
        }

        class ChildIterator {
        private:
            size_t ix;
            NodePtrType node; // node being iterated over
        public:
            ChildIterator(NodePtrType _node) { node = _node; ix = 0; }
            bool hasNext() { return ix < 2; }
            NodePtrType next() { return (ix++ == 1) ? DATA_STRUCTURE_T::get_left(node) : DATA_STRUCTURE_T::get_right(node); }
        };
        
        static bool isLeaf(NodePtrType node) { return DATA_STRUCTURE_T::get_left(node) == NULL && DATA_STRUCTURE_T::get_right(node) == NULL; }
        static ChildIterator getChildIterator(NodePtrType node) { return ChildIterator(node); }
        static size_t getNumChildren(NodePtrType node) { return isLeaf(node) ? 0 : 2; }
        static size_t getNumKeys(NodePtrType node) { return isLeaf(node); }
        static size_t getSumOfKeys(NodePtrType node) { return isLeaf(node) ? (size_t) node->key : 0; }
    };
    TreeStats<NodeHandler> * createTreeStats(const K& _minKey, const K& _maxKey) {
        return new TreeStats<NodeHandler>(new NodeHandler(_minKey, _maxKey), DATA_STRUCTURE_T::get_left(DATA_STRUCTURE_T::get_left(tree->get_root())), true);
    }
#endif
};

#endif
