//
// Created by lwh on 19-3-14.
//

#ifndef BST_GLOCK_H
#define BST_GLOCK_H

#include "record_manager.h"
#include "random_fnv1a.h"
#include "locks_impl.h"

namespace bst_glock_ns {

#define nodeptr Node<K,V> * volatile

    template<class K, class V>
    class Node {
    public:
        V value;
        K key;
        nodeptr left;
        nodeptr right;
    };

    template<class K, class V, class Compare, class RecManager>
    class bst_glock {
    private:
        PAD;
        RecManager *const recmgr;
        PAD;
        volatile int lock;
        PAD;
        nodeptr root;        // actually const
        Compare cmp;
        PAD;

        inline nodeptr createNode(const int tid, const K &key, const V &value, nodeptr const left,
                                  nodeptr const right) {
            nodeptr newnode = recmgr->template allocate<Node<K, V> >(tid);
            if (newnode == NULL) {
                COUTATOMICTID("ERROR: could not allocate node" << std::endl);
                exit(-1);
            }
#ifdef __HANDLE_STATS
            GSTATS_APPEND(tid, node_allocated_addresses, ((long long) newnode)%(1<<12));
#endif
            newnode->key = key;
            newnode->value = value;
            newnode->left = left;
            newnode->right = right;
            return newnode;
        }

        const V doInsert(const int tid, const K &key, const V &val, bool onlyIfAbsent);

        int init[MAX_THREADS_POW2] = {0,};
        PAD;

    public:
        const K NO_KEY;
        const V NO_VALUE;
        PAD;

        /**
         * This function must be called once by each thread that will
         * invoke any functions on this class.
         *
         * It must be okay that we do this with the main thread and later with another thread!!!
         */
        void initThread(const int tid) {
            if (init[tid]) return; else init[tid] = !init[tid];
            recmgr->initThread(tid);
        }

        void deinitThread(const int tid) {
            if (!init[tid]) return; else init[tid] = !init[tid];
            recmgr->deinitThread(tid);
        }

        bst_glock(const K _NO_KEY,
                  const V _NO_VALUE,
                  const int numProcesses)
                : NO_KEY(_NO_KEY), NO_VALUE(_NO_VALUE), recmgr(new RecManager(numProcesses)) {
            lock = 0;

            VERBOSE DEBUG COUTATOMIC("constructor bst_glock" << std::endl);
            cmp = Compare();

            const int tid = 0;
            initThread(tid);

            recmgr->endOp(tid); // enter an initial quiescent state.
            nodeptr rootleft = createNode(tid, NO_KEY, NO_VALUE, NULL, NULL);
            nodeptr _root = createNode(tid, NO_KEY, NO_VALUE, rootleft, NULL);
            root = _root;
        }

        void dfsDeallocateBottomUp(nodeptr const u, int *numNodes) {
            if (u == NULL) return;
            if (u->left != NULL) {
                dfsDeallocateBottomUp(u->left, numNodes);
                dfsDeallocateBottomUp(u->right, numNodes);
            }
            MEMORY_STATS ++(*numNodes);
            const int tid = 0;
            recmgr->deallocate(tid, u);
        }

        ~bst_glock() {
            VERBOSE DEBUG COUTATOMIC("destructor bst_glock");
            // free every node and scx record currently in the data structure.
            // an easy DFS, freeing from the leaves up, handles all nodes.
            // cleaning up scx records is a little bit harder if they are in progress or aborted.
            // they have to be collected and freed only once, since they can be pointed to by many nodes.
            // so, we keep them in a set, then free each set element at the end.
            int numNodes = 0;
            dfsDeallocateBottomUp(root, &numNodes);
            VERBOSE DEBUG COUTATOMIC(" deallocated nodes " << numNodes << std::endl);
            recmgr->printStatus();
            delete recmgr;
        }

        const V insert(const int tid, const K &key, const V &val) { return doInsert(tid, key, val, false); }

        const V insertIfAbsent(const int tid, const K &key, const V &val) { return doInsert(tid, key, val, true); }

        const std::pair<V, bool> erase(const int tid, const K &key);

        const std::pair<V, bool> find(const int tid, const K &key);

        int
        rangeQuery(const int tid, const K &lo, const K &hi, K *const resultKeys, V *const resultValues) { return 0; }

        bool contains(const int tid, const K &key) { return find(tid, key).second; }

        RecManager *debugGetRecMgr() { return recmgr; }

        nodeptr debug_getEntryPoint() { return root; }
    };

}

#endif
