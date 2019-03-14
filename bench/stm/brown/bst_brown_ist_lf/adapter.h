/**
 * Implementation of a lock-free interpolation search tree.
 * Trevor Brown, 2018.
 */

#ifndef DS_ADAPTER_H
#define DS_ADAPTER_H

#include <iostream>
#include "errors.h"
#include "random_fnv1a.h"
#include "brown_ist_lf_impl.h"

#ifdef USE_TREE_STATS
#   include "tree_stats.h"
#endif

#define RECORD_MANAGER_T record_manager<Reclaim, Alloc, Pool, Node<K,V>, KVPair<K,V>, RebuildOperation<K,V>, MultiCounter>
#define DATA_STRUCTURE_T istree<K, V, Interpolator<K>, RECORD_MANAGER_T>

template<typename T>
struct ValidAllocatorTest {
    static constexpr bool value = false;
};

template<typename T>
struct ValidAllocatorTest<allocator_new < T>> {
static constexpr bool value = true;
};

template<typename Alloc>
static bool isValidAllocator(void) {
    return ValidAllocatorTest<Alloc>::value;
}

template<typename K>
class Interpolator {
public:
    bool less(const K &a, const K &b);

    double interpolate(const K &key, const K &rangeLeft, const K &rangeRight);
};

template<>
class Interpolator<long long> {
public:
    int compare(const long long &a, const long long &b) {
        return (a < b) ? -1 : (a > b) ? 1 : 0;
    }
//    double interpolate(const long long& key, const long long& rangeLeft, const long long& rangeRight) {
//        if (rangeRight == rangeLeft) return 0;
//        return ((double) key - (double) rangeLeft) / ((double) rangeRight - (double) rangeLeft);
//    }
};

template<typename K, typename V, class Reclaim = reclaimer_debra <K>, class Alloc = allocator_new <K>, class Pool = pool_none <K>>
class ds_adapter {
private:
    DATA_STRUCTURE_T *
    const ds;

public:
    ds_adapter(const int NUM_THREADS,
               const K &unused1,
               const K &KEY_MAX,
               const V &NO_VALUE,
               RandomFNV1A *const unused3)
            : ds(new DATA_STRUCTURE_T (NUM_THREADS, KEY_MAX, NO_VALUE)) {
        if (!isValidAllocator<Alloc>()) {
            setbench_error("This data structure must be used with allocator_new.")
        }
        if (NUM_THREADS > MAX_THREADS_POW2) {
            setbench_error("NUM_THREADS exceeds MAX_THREADS_POW2");
        }
    }

    ds_adapter(const int NUM_THREADS, const K &unused1, const K &KEY_MAX, const V &NO_VALUE, RandomFNV1A *const unused3,
               const K *const initKeys, const V *const initValues, const size_t initNumKeys,
               const size_t initConstructionSeed /* note: randomness is used to ensure good tree structure whp */
    )
            : ds(
            new DATA_STRUCTURE_T (initKeys, initValues, initNumKeys, initConstructionSeed, NUM_THREADS, KEY_MAX, NO_VALUE)) {
        if (!isValidAllocator<Alloc>()) {
            setbench_error("This data structure must be used with allocator_new.")
        }
        if (NUM_THREADS > MAX_THREADS_POW2) {
            setbench_error("NUM_THREADS exceeds MAX_THREADS_POW2");
        }
    }

    ~ds_adapter() {
        delete ds;
    }

    void *getNoValue() {
        return ds->NO_VALUE;
    }

    void initThread(const int tid) {
        ds->initThread(tid);
    }

    void deinitThread(const int tid) {
        ds->deinitThread(tid);
    }

    bool contains(const int tid, const K &key) {
        return ds->contains(tid, key);
    }

    void *insert(const int tid, const K &key, void *const val) {
        return ds->insert(tid, key, val);
    }

    void *insertIfAbsent(const int tid, const K &key, void *const val) {
        return ds->insertIfAbsent(tid, key, val);
    }

    void *erase(const int tid, const K &key) {
        return ds->erase(tid, key);
    }

    void *find(const int tid, const K &key) {
        return ds->find(tid, key);
    }

    int rangeQuery(const int tid, const K &lo, const K &hi, K *const resultKeys, void **const resultValues) {
        setbench_error("not implemented");
    }

    void printSummary() {
        auto recmgr = ds->debugGetRecMgr();
        recmgr->printStatus();
    }

    bool validateStructure() {
//        ds->debugGVPrint();
        return true;
    }

    void printObjectSizes() {
        std::cout << "size_node=" << (sizeof(Node < K, V >)) << std::endl;
    }

#ifdef USE_TREE_STATS
    class NodeHandler {
    public:
        typedef casword_t NodePtrType;
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
            bool hasNext() { return ix < CASWORD_TO_NODE(node)->degree; }
            NodePtrType next() { return CASWORD_TO_NODE(node)->ptr(ix++); }
        };

        static bool isLeaf(NodePtrType node) { return IS_KVPAIR(node) || IS_VAL(node); }
        static ChildIterator getChildIterator(NodePtrType node) { return ChildIterator(node); }
        static size_t getNumChildren(NodePtrType node) { return isLeaf(node) ? 0 : CASWORD_TO_NODE(node)->degree; }
        static size_t getNumKeys(NodePtrType node) {
            if (IS_KVPAIR(node)) return 1;
            if (IS_VAL(node)) return 0;
            assert(IS_NODE(node));
            auto n = CASWORD_TO_NODE(node);
            assert(IS_EMPTY_VAL(n->ptr(0)) || !IS_VAL(n->ptr(0)));
            size_t ret = 0;
            for (int i=1; i < n->degree; ++i) {
                if (!IS_EMPTY_VAL(n->ptr(i)) && IS_VAL(n->ptr(i))) ++ret;
            }
            return ret;
        }
        static size_t getSumOfKeys(NodePtrType node) {
            if (IS_KVPAIR(node)) return (size_t) CASWORD_TO_KVPAIR(node)->k;
            if (IS_VAL(node)) return 0;
            assert(IS_NODE(node));
            auto n = CASWORD_TO_NODE(node);
            assert(IS_EMPTY_VAL(n->ptr(0)) || !IS_VAL(n->ptr(0)));
            size_t ret = 0;
            for (int i=1; i < n->degree; ++i) {
                if (!IS_EMPTY_VAL(n->ptr(i)) && IS_VAL(n->ptr(i))) ret += (size_t) n->key(i-1);
            }
            return ret;
        }
    };
    TreeStats<NodeHandler> * createTreeStats(const K& _minKey, const K& _maxKey) {
        return new TreeStats<NodeHandler>(new NodeHandler(_minKey, _maxKey), NODE_TO_CASWORD(ds->debug_getEntryPoint()), true);
    }
#endif
};

#endif
