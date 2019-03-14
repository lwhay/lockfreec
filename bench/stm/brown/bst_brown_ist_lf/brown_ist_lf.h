//
// Created by lwh on 19-3-14.
//

#ifndef ISTREE_H
#define ISTREE_H

#define PREFILL_SEQUENTIAL_BUILD_FROM_ARRAY
#define IST_USE_MULTICOUNTER_AT_ROOT
//#define NO_REBUILDING
//#define PAD_CHANGESUM
//#define SEQUENTIAL_MARK_AND_COUNT
#define MAX_ACCEPTABLE_LEAF_SIZE (48)

#define GV_FLIP_RECORDS

#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <limits>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <sys/types.h>
#include "random_fnv1a.h"
#include "record_manager.h"
#include "dcss_impl.h"

#ifdef IST_USE_MULTICOUNTER_AT_ROOT

#   include "multi_counter.h"

#endif

// for fields Node::ptr(...)
#define TYPE_MASK                   (0x6ll)
#define DCSS_BITS                   (1)
#define TYPE_BITS                   (2)
#define TOTAL_BITS                  (DCSS_BITS+TYPE_BITS)
#define TOTAL_MASK                  (0x7ll)

#define NODE_MASK                   (0x0ll) /* no need to use this... 0 mask is implicit */
#define IS_NODE(x)                  (((x)&TYPE_MASK)==NODE_MASK)
#define CASWORD_TO_NODE(x)          ((Node<K,V> *) (x))
#define NODE_TO_CASWORD(x)          ((casword_t) (x))

#define KVPAIR_MASK                 (0x2ll) /* 0x1 is used by DCSS */
#define IS_KVPAIR(x)                (((x)&TYPE_MASK)==KVPAIR_MASK)
#define CASWORD_TO_KVPAIR(x)        ((KVPair<K,V> *) ((x)&~TYPE_MASK))
#define KVPAIR_TO_CASWORD(x)        ((casword_t) (((casword_t) (x))|KVPAIR_MASK))

#define REBUILDOP_MASK              (0x4ll)
#define IS_REBUILDOP(x)             (((x)&TYPE_MASK)==REBUILDOP_MASK)
#define CASWORD_TO_REBUILDOP(x)     ((RebuildOperation<K,V> *) ((x)&~TYPE_MASK))
#define REBUILDOP_TO_CASWORD(x)     ((casword_t) (((casword_t) (x))|REBUILDOP_MASK))

#define VAL_MASK                    (0x6ll)
#define IS_VAL(x)                   (((x)&TYPE_MASK)==VAL_MASK)
#define CASWORD_TO_VAL(x)           ((V) ((x)>>TOTAL_BITS))
#define VAL_TO_CASWORD(x)           ((casword_t) ((((casword_t) (x))<<TOTAL_BITS)|VAL_MASK))

#define EMPTY_VAL_TO_CASWORD        (((casword_t) ~TOTAL_MASK) | VAL_MASK)
#define IS_EMPTY_VAL(x)             (((casword_t) (x)) == EMPTY_VAL_TO_CASWORD)

// for field Node::dirty
// note: dirty finished should imply dirty started!
#define DIRTY_STARTED_MASK          (0x1ll)
#define DIRTY_FINISHED_MASK         (0x2ll)
#define IS_DIRTY_STARTED(x)         ((x)&DIRTY_STARTED_MASK)
#define IS_DIRTY_FINISHED(x)        ((x)&DIRTY_FINISHED_MASK)
#define SUM_TO_DIRTY_FINISHED(x)    (((x)<<2)|DIRTY_FINISHED_MASK|DIRTY_STARTED_MASK)
#define DIRTY_FINISHED_TO_SUM(x)    ((x)>>2)

// constants for rebuilding
#define REBUILD_FRACTION            (0.25)
#define EPS                         (0.25)

static __thread RandomFNV1A *myRNG = NULL;

enum UpdateType {
    InsertIfAbsent, InsertReplace, Erase
};

template<typename K, typename V>
struct Node {
    size_t volatile degree;
    K minKey;                   // field not *technically* needed (used to avoid loading extra cache lines for interpolationSearch in the common case, buying for time for prefilling while interpolation arithmetic occurs)
    K maxKey;                   // field not *technically* needed (same as above)
    size_t capacity;            // field likely not needed (but convenient and good for debug asserts)
    size_t initSize;
    size_t volatile dirty;      // 2-LSBs are marked by markAndCount; also stores the number of pairs in a subtree as recorded by markAndCount (see SUM_TO_DIRTY_FINISHED and DIRTY_FINISHED_TO_SUM)
    size_t volatile nextMarkAndCount; // facilitates recursive-collaborative markAndCount() by allowing threads to dynamically soft-partition subtrees (NOT workstealing/exclusive access---this is still a lock-free mechanism)
#ifdef PAD_CHANGESUM
    PAD;
#endif
    volatile size_t changeSum;  // could be merged with initSize above (subtract make initSize 1/4 of what it would normally be, then subtract from it instead of incrementing changeSum, and rebuild when it hits zero)
#ifdef IST_USE_MULTICOUNTER_AT_ROOT
    MultiCounter *externalChangeCounter; // NULL for all nodes except the root (or top few nodes), and supercedes changeSum when non-NULL.
#endif
    // unlisted fields: capacity-1 keys of type K followed by capacity values/pointers of type casword_t
    // the values/pointers have tags in their 3 LSBs so that they satisfy either IS_NODE, IS_KVPAIR, IS_REBUILDOP or IS_VAL

    inline K *keyAddr(const int ix) {
        K *const firstKey = ((K *) (((char *) this) + sizeof(Node<K, V>)));
        return &firstKey[ix];
    }

    inline K &key(const int ix) {
        assert(ix >= 0);
        assert(ix < degree - 1);
        return *keyAddr(ix);
    }

    // conceptually returns &node.ptrs[ix]
    inline casword_t volatile *ptrAddr(const int ix) {
        assert(ix >= 0);
        assert(ix < degree);
        K *const firstKeyAfter = keyAddr(capacity - 1);
        casword_t *const firstPtr = (casword_t *) firstKeyAfter;
        return &firstPtr[ix];
    }

    // conceptually returns node.ptrs[ix]
    inline casword_t volatile ptr(const int ix) {
        return *ptrAddr(ix);
    }

    inline void incrementChangeSum(const int tid, RandomFNV1A *rng) {
#ifdef IST_USE_MULTICOUNTER_AT_ROOT
        if (likely(externalChangeCounter == NULL)) {
            __sync_fetch_and_add(&changeSum, 1);
        } else {
            externalChangeCounter->inc(tid, rng);
        }
#else
        __sync_fetch_and_add(&changeSum, 1);
#endif
    }

    inline size_t readChangeSum(const int tid, RandomFNV1A *rng) {
#ifdef IST_USE_MULTICOUNTER_AT_ROOT
        if (likely(externalChangeCounter == NULL)) {
            return changeSum;
        } else {
            return externalChangeCounter->readFast(tid, rng);
        }
#else
        return changeSum;
#endif
    }
};

template<typename K, typename V>
struct RebuildOperation {
    Node<K, V> *candidate;
    Node<K, V> *parent;
    size_t index;
    size_t depth;
    casword_t volatile newRoot;

    RebuildOperation(Node<K, V> *_candidate, Node<K, V> *_parent, size_t _index, size_t _depth)
            : candidate(_candidate), parent(_parent), index(_index), depth(_depth), newRoot(NODE_TO_CASWORD(NULL)) {}
};

template<typename K, typename V>
struct KVPair {
    K k;
    V v;
};

template<typename V>
struct IdealSubtree {
    casword_t ptr;
    V minVal;
};

template<typename K, typename V, class Interpolate, class RecManager>
class istree {
private:
    PAD;
    RecManager *const recordmgr;
    dcssProvider *const prov;
    Interpolate cmp;

    Node<K, V> *root;

#define arraycopy(src, srcStart, dest, destStart, len) \
        for (int ___i=0;___i<(len);++___i) { \
            (dest)[(destStart)+___i] = (src)[(srcStart)+___i]; \
        }

    size_t markAndCount(const int tid, const casword_t ptr);

    void rebuild(const int tid, Node<K, V> *rebuildRoot, Node<K, V> *parent, int index, const size_t depth);

    void helpRebuild(const int tid, RebuildOperation<K, V> *op);

    int interpolationSearch(const int tid, const K &key, Node<K, V> *const node);

    V doUpdate(const int tid, const K &key, const V &val, UpdateType t);

    Node<K, V> *createNode(const int tid, const int degree);

    Node<K, V> *createLeaf(const int tid, KVPair<K, V> *pairs, int numPairs);

    Node<K, V> *createMultiCounterNode(const int tid, const int degree);

    KVPair<K, V> *createKVPair(const int tid, const K &key, const V &value);

    void debugPrintWord(std::ofstream &ofs, casword_t w) {
        //ofs<<(void *) (w&~TOTAL_MASK)<<"("<<(IS_REBUILDOP(w) ? "r" : IS_VAL(w) ? "v" : "n")<<")";
        //ofs<<(IS_REBUILDOP(w) ? "RebuildOp *" : IS_VAL(w) ? "V" : "Node *");
        //ofs<<(IS_REBUILDOP(w) ? "r" : IS_VAL(w) ? "v" : "n");
    }

    void debugGVPrint(std::ofstream &ofs, casword_t w, size_t depth, int *numPointers) {
        if (IS_KVPAIR(w)) {
            auto pair = CASWORD_TO_KVPAIR(w);
            ofs << "\"" << pair << "\" [" << std::endl;
//            if (pair->empty) {
//                ofs<<"label = \"<f0> deleted\""<<std::endl;
//            } else {
            ofs << "label = \"<f0> " << pair->k << "\"" << std::endl;
//            }
            ofs << "shape = \"record\"" << std::endl;
            ofs << "];" << std::endl;
        } else if (IS_REBUILDOP(w)) {
            auto op = CASWORD_TO_REBUILDOP(w);
            ofs << "\"" << op << "\" [" << std::endl;
            ofs << "label = \"<f0> rebuild";
            debugPrintWord(ofs, NODE_TO_CASWORD(op->candidate));
            ofs << "\"" << std::endl;
            ofs << "shape = \"record\"" << std::endl;
            ofs << "];" << std::endl;

            ofs << "\"" << op << "\":f0 -> \"" << op->candidate << "\":f0 [" << std::endl;
            ofs << "id = " << ((*numPointers)++) << std::endl;
            ofs << "];" << std::endl;
            debugGVPrint(ofs, NODE_TO_CASWORD(op->candidate), 1 + depth, numPointers);
        } else {
            assert(IS_NODE(w));
            const int tid = 0;
            auto node = CASWORD_TO_NODE(w);
            ofs << "\"" << node << "\" [" << std::endl;
#ifdef GV_FLIP_RECORDS
            ofs << "label = \"{";
#else
            ofs<<"label = \"";
#endif
            int numFixedFields = 0;
            //ofs<<"<f"<<(numFixedFields++)<<"> c:"<<node->capacity;
            ofs << "<f" << (numFixedFields++) << "> d:" << node->degree << "/" << node->capacity;
            ofs << " | <f" << (numFixedFields++) << "> is:" << node->initSize;
            ofs << " | <f" << (numFixedFields++) << "> cs:" << node->changeSum;

            if (node->externalChangeCounter) {
                ofs << " | <f" << (numFixedFields++) << "> ext";
            } else {
                ofs << " | <f" << (numFixedFields++) << "> -";
            }

            auto dirty = node->dirty;
            ofs << " | <f" << (numFixedFields++) << "> m:" << DIRTY_FINISHED_TO_SUM(dirty)
                << (IS_DIRTY_STARTED(dirty) ? "s" : "") << (IS_DIRTY_FINISHED(dirty) ? "f" : "");
#define FIELD_PTR(i) (numFixedFields+2*(i))
#define FIELD_KEY(i) (FIELD_PTR(i)-1)
            for (int i = 0; i < node->degree; ++i) {
                if (i > 0) ofs << " | <f" << FIELD_KEY(i) << "> " << node->key(i - 1);
                casword_t targetWord = prov->readPtr(tid, node->ptrAddr(i));
                ofs << " | <f" << FIELD_PTR(i) << "> ";
                if (IS_EMPTY_VAL(targetWord)) { ofs << "e"; }
                else if (IS_VAL(targetWord)) { ofs << "v"; }
                //debugPrintWord(ofs, targetWord);
            }
#ifdef GV_FLIP_RECORDS
            ofs << "}\"" << std::endl;
#else
            ofs<<"\""<<std::endl;
#endif
            ofs << "shape = \"record\"" << std::endl;
            ofs << "];" << std::endl;

            if (node->externalChangeCounter) {
                ofs << "\"" << node->externalChangeCounter << "\" [" << std::endl;
                ofs << "label= \"";
                ofs << "<f0> cnt=" << (node->externalChangeCounter->readAccurate());
                ofs << "\"" << std::endl;
                ofs << "shape = \"record\"" << std::endl;
                ofs << "];" << std::endl;

                ofs << "\"" << node << "\":f3 -> \"" << node->externalChangeCounter << "\":f0 [" << std::endl;
                ofs << "id = " << ((*numPointers)++) << std::endl;
                ofs << "];" << std::endl;
            }

            for (int i = 0; i < node->degree; ++i) {
                casword_t targetWord = prov->readPtr(tid, node->ptrAddr(i));
                if (IS_VAL(targetWord)) continue;

                void *target = (void *) (targetWord & ~TOTAL_MASK);
                ofs << "\"" << node << "\":f" << FIELD_PTR(i) << " -> \"" << target << "\":f0 [" << std::endl;
                ofs << "id = " << ((*numPointers)++) << std::endl;
                ofs << "];" << std::endl;
            }

            for (int i = 0; i < node->degree; ++i) {
                casword_t targetWord = prov->readPtr(tid, node->ptrAddr(i));
                if (IS_VAL(targetWord)) continue;

                debugGVPrint(ofs, targetWord, 1 + depth, numPointers);
            }
        }
    }

public:
    void debugGVPrint(const int tid = 0) {
        std::stringstream ss;
        ss << "gvinput_tid" << tid << ".dot";
        auto s = ss.str();
        printf("dumping tree to dot file \"%s\"\n", s.c_str());
        std::ofstream ofs;
        ofs.open(s, std::ofstream::out);
//        ofs<<"digraph g {"<<std::endl<<"graph ["<<std::endl<<"rankdir = \"LR\""<<std::endl<<"];"<<std::endl;
        ofs << "digraph g {" << std::endl << "graph [" << std::endl << "rankdir = \"TB\"" << std::endl << "];"
            << std::endl;
        ofs << "node [" << std::endl << "fontsize = \"16\"" << std::endl << "shape = \"ellipse\"" << std::endl << "];"
            << std::endl;
        ofs << "edge [" << std::endl << "];" << std::endl;

        int numPointers = 0;
        debugGVPrint(ofs, NODE_TO_CASWORD(root), 0, &numPointers);

        ofs << "}" << std::endl;
        ofs.close();
    }

private:
    void freeNode(const int tid, Node<K, V> *node, bool retire) {
        if (retire) {
#ifdef IST_USE_MULTICOUNTER_AT_ROOT
            if (node->externalChangeCounter) {
//                GSTATS_ADD(tid, num_multi_counter_node_retired, 1);
                recordmgr->retire(tid, node->externalChangeCounter);
            }
#endif
            recordmgr->retire(tid, node);
        } else {
#ifdef IST_USE_MULTICOUNTER_AT_ROOT
            if (node->externalChangeCounter) {
//                GSTATS_ADD(tid, num_multi_counter_node_deallocated, 1);
                recordmgr->deallocate(tid, node->externalChangeCounter);
            }
#endif
            recordmgr->deallocate(tid, node);
        }
    }

    void freeSubtree(const int tid, casword_t ptr, bool retire) {
        if (unlikely(IS_KVPAIR(ptr))) {
            if (retire) {
                recordmgr->retire(tid, CASWORD_TO_KVPAIR(ptr));
            } else {
                recordmgr->deallocate(tid, CASWORD_TO_KVPAIR(ptr));
            }
        } else if (IS_REBUILDOP(ptr)) {
            auto op = CASWORD_TO_REBUILDOP(ptr);
            freeSubtree(tid, NODE_TO_CASWORD(op->candidate), retire);
            if (retire) {
                recordmgr->retire(tid, op);
            } else {
                recordmgr->deallocate(tid, op);
            }
        } else if (IS_NODE(ptr) && ptr != NODE_TO_CASWORD(NULL)) {
            auto node = CASWORD_TO_NODE(ptr);
            for (int i = 0; i < node->degree; ++i) {
                auto child = prov->readPtr(tid, node->ptrAddr(i));
                freeSubtree(tid, child, retire);
            }
            freeNode(tid, node, retire);
        }
    }

    /**
     * recursive ideal ist construction
     * divide and conquer,
     * constructing from a particular set of k of pairs,
     * create one node with degree ceil(sqrt(k)),
     * then recurse on each child (partitioning the pairs as evenly as possible),
     * and attach the resulting ists as children of this node,
     * and return this node.
     * if k is at most 48, there are no recursive calls:
     * the key-value pairs are simply encoded in the node.)
     */

    class IdealBuilder {
    private:
        static const int UPPER_LIMIT_DEPTH = 16;
        size_t initNumKeys;
        istree<K, V, Interpolate, RecManager> *ist;
        size_t depth;
        KVPair<K, V> *pairs;
        size_t pairsAdded;
        casword_t tree;

        Node<K, V> *build(const int tid, KVPair<K, V> *pset, int psetSize, const size_t currDepth,
                          casword_t volatile *constructingSubtree) {
            if (*constructingSubtree != NODE_TO_CASWORD(NULL)) {
                GSTATS_ADD_IX(tid, num_bail_from_build_at_depth, 1, (currDepth > 9 ? 9 : currDepth));
                return NODE_TO_CASWORD(NULL); // bail early if tree was already constructed by someone else
            }

            if (psetSize <= MAX_ACCEPTABLE_LEAF_SIZE) {
                return ist->createLeaf(tid, pset, psetSize);
            } else {
                double numChildrenD = std::sqrt((double) psetSize);
                size_t numChildren = (size_t) std::ceil(numChildrenD);

                size_t childSize = psetSize / (size_t) numChildren;
                size_t remainder = psetSize % numChildren;
                // remainder is the number of children with childSize+1 pair subsets
                // (the other (numChildren - remainder) children have childSize pair subsets)

#ifdef IST_USE_MULTICOUNTER_AT_ROOT
                Node<K, V> *node = NULL;
                if (currDepth <= 1) {
                    node = ist->createMultiCounterNode(tid, numChildren);
                } else {
                    node = ist->createNode(tid, numChildren);
                }
#else
                auto node = ist->createNode(tid, numChildren);
#endif
                node->degree = numChildren;
                node->initSize = psetSize;

                KVPair<K, V> *childSet = pset;
                for (int i = 0; i < numChildren; ++i) {
                    int sz = childSize + (i < remainder);
                    auto child = build(tid, childSet, sz, 1 + currDepth, constructingSubtree);

                    *node->ptrAddr(i) = NODE_TO_CASWORD(child);
                    if (i > 0) {
                        assert(child == NODE_TO_CASWORD(NULL) || child->degree > 1);
                        node->key(i - 1) = childSet[0].k;
                    }
#ifndef NDEBUG
                    assert(i < 2 || node->key(i-1) > node->key(i-2));
#endif
                    childSet += sz;
                }
                node->minKey = node->key(0);
                node->maxKey = node->key(node->degree - 2);
                assert(node->degree <= node->capacity);
                return node;
            }
        }

    public:
        IdealBuilder(istree<K, V, Interpolate, RecManager> *_ist, const size_t _initNumKeys, const size_t _depth) {
            initNumKeys = _initNumKeys;
            ist = _ist;
            depth = _depth;
            pairs = new KVPair<K, V>[initNumKeys];
            pairsAdded = 0;
            tree = (casword_t) NULL;
        }

        ~IdealBuilder() {
            delete[] pairs;
        }

        void addKV(const int tid, const K &key, const V &value) {
            pairs[pairsAdded++] = {key, value};
#ifndef NDEBUG
            if (pairsAdded > initNumKeys) {
                printf("tid=%d key=%lld pairsAdded=%lld initNumKeys=%lld\n", tid, key, pairsAdded, initNumKeys);
                ist->debugGVPrint(tid);
            }
#endif
            assert(pairsAdded <= initNumKeys);
        }

        casword_t getCASWord(const int tid, casword_t volatile *constructingSubtree) {
            if (*constructingSubtree != NODE_TO_CASWORD(NULL)) return NODE_TO_CASWORD(NULL);
#ifndef NDEBUG
                if (pairsAdded != initNumKeys) {
                printf("tid=%d pairsAdded=%lld initNumKeys=%lld\n", tid, pairsAdded, initNumKeys);
                if (pairsAdded > 0) printf("tid=%d key[0]=%lld\n", tid, pairs[0].k);
                ist->debugGVPrint(tid);
            }
#endif
            assert(pairsAdded == initNumKeys);
#ifndef NDEBUG
            for (int i=1;i<pairsAdded;++i) {
                if (pairs[i].k <= pairs[i-1].k) {
                    std::cout<<pairs[i].k<<" is less than or equal to the previous key "<<pairs[i-1].k<<std::endl;
                    ist->debugGVPrint();
                }
                assert(pairs[i].k > pairs[i-1].k);
            }
#endif
            if (!tree) {
                if (unlikely(pairsAdded == 0)) {
                    tree = EMPTY_VAL_TO_CASWORD;
                } else if (unlikely(pairsAdded == 1)) {
                    tree = KVPAIR_TO_CASWORD(ist->createKVPair(tid, pairs[0].k, pairs[0].v));
                } else {
                    tree = NODE_TO_CASWORD(build(tid, pairs, pairsAdded, depth, constructingSubtree));
                }
            }
            if (*constructingSubtree != NODE_TO_CASWORD(NULL)) {
                ist->freeSubtree(tid, tree, false);
                return NODE_TO_CASWORD(NULL);
            }
            return tree;
        }

        K getMinKey() {
            assert(pairsAdded > 0);
            return pairs[0].k;
        }
    };

    void addKVPairs(const int tid, casword_t ptr, IdealBuilder *b);

    void addKVPairsSubset(const int tid, RebuildOperation<K, V> *op, Node<K, V> *node, size_t *numKeysToSkip,
                          size_t *numKeysToAdd, size_t depth, IdealBuilder *b, casword_t volatile *constructingSubtree);

    casword_t createIdealConcurrent(const int tid, RebuildOperation<K, V> *op, const size_t keyCount);

    void
    subtreeBuildAndReplace(const int tid, RebuildOperation<K, V> *op, Node<K, V> *parent, size_t ix, size_t childSize,
                           size_t remainder);

    int init[MAX_THREADS_POW2] = {0,};
public:
    const K INF_KEY;
    const V NO_VALUE;
    const int NUM_PROCESSES;
    PAD;

    void initThread(const int tid) {
        if (myRNG == NULL) myRNG = new RandomFNV1A(rand());
        if (init[tid]) return; else init[tid] = !init[tid];

        prov->initThread(tid);
        recordmgr->initThread(tid);
    }

    void deinitThread(const int tid) {
        if (myRNG != NULL) {
            delete myRNG;
            myRNG = NULL;
        }
        if (!init[tid]) return; else init[tid] = !init[tid];

        prov->deinitThread(tid);
        recordmgr->deinitThread(tid);
    }

    istree(const int numProcesses, const K infinity, const V noValue
    )
            : recordmgr(new RecManager(numProcesses, SIGQUIT)), prov(new dcssProvider(numProcesses)), INF_KEY(infinity),
              NO_VALUE(noValue), NUM_PROCESSES(numProcesses) {
        srand(time(0)); // for seeding per-thread RNGs in initThread
        cmp = Interpolate();

        const int tid = 0;
        initThread(tid);

        Node<K, V> *_root = createNode(tid, 1);
        _root->degree = 1;
        _root->minKey = INF_KEY;
        _root->maxKey = INF_KEY;
        *_root->ptrAddr(0) = EMPTY_VAL_TO_CASWORD;

        root = _root;
    }

    istree(const K *const initKeys, const V *const initValues, const size_t initNumKeys,
           const size_t initConstructionSeed, const int numProcesses, const K infinity, const V noValue
    )
            : recordmgr(new RecManager(numProcesses, SIGQUIT)), prov(new dcssProvider(numProcesses)), INF_KEY(infinity),
              NO_VALUE(noValue), NUM_PROCESSES(numProcesses) {
        cmp = Interpolate();

        const int tid = 0;
        initThread(tid);

        Node<K, V> *_root = createNode(tid, 1);
        _root->degree = 1;
        root = _root;

        IdealBuilder b(this, initNumKeys, 0);
        for (size_t keyIx = 0; keyIx < initNumKeys; ++keyIx) {
            b.addKV(tid, initKeys[keyIx], initValues[keyIx]);
        }
        casword_t dummy = NODE_TO_CASWORD(NULL);
        *root->ptrAddr(0) = b.getCASWord(tid, &dummy);
    }

    ~istree() {
        if (myRNG != NULL) {
            delete myRNG;
            myRNG = NULL;
        }
//        debugGVPrint();
        freeSubtree(0, NODE_TO_CASWORD(root), false);
////            COUTATOMIC("main thread: deleted tree containing "<<nodes<<" nodes"<<std::endl);
        recordmgr->printStatus();
        delete prov;
        delete recordmgr;
    }

    Node<K, V> *debug_getEntryPoint() { return root; }

    V find(const int tid, const K &key);

    bool contains(const int tid, const K &key) {
        return find(tid, key);
    }

    V insert(const int tid, const K &key, const V &val) {
        return doUpdate(tid, key, val, InsertReplace);
    }

    V insertIfAbsent(const int tid, const K &key, const V &val) {
        return doUpdate(tid, key, val, InsertIfAbsent);
    }

    V erase(const int tid, const K &key) {
        return doUpdate(tid, key, NO_VALUE, Erase);
    }

    RecManager *const debugGetRecMgr() {
        return recordmgr;
    }
};

#endif    /* ISTREE_H */
