//
// Created by lwh on 19-3-14.
//


#ifndef CCAVL_H
#define CCAVL_H

//#if  (INDEX_STRUCT == IDX_CCAVL_SPIN)
//#define SPIN_LOCK
//#elif   (INDEX_STRUCT == IDX_CCAVL_BASELINE)
//#define BASELINE
////uses pthread mutex
//#else
//#error
//#endif

//#ifdef SPIN_LOCK
typedef pthread_spinlock_t ptlock_t;
#define lock_size               sizeof(ptlock_t)
#define mutex_init(lock)        pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE)
#define mutex_destroy(lock)     pthread_spin_destroy(lock)
#define mutex_lock(lock)        pthread_spin_lock(lock)
#define mutex_unlock(lock)      pthread_spin_unlock(lock)
//#else
//typedef pthread_mutex_t ptlock_t;
//#define lock_size               sizeof(ptlock_t)
//#define mutex_init(lock)        pthread_mutex_init(lock, NULL)
//#define mutex_destroy(lock)     pthread_mutex_destroy(lock)
//#define mutex_lock(lock)        pthread_mutex_lock(lock)
//#define mutex_unlock(lock)      pthread_mutex_unlock(lock)
//#endif

#define lock_mb() asm volatile("":::"memory")

#define IMPLEMENTED 1
#define MAX(a, b) ( (a) > (b) ? (a) : (b) )
#define ABS(a) ( (a) > 0 ? (a)  : -(a) )

typedef unsigned long long version_t;

#ifndef PAD_SIZE
#define PAD_SIZE 128
#endif

template<typename skey_t, typename sval_t>
struct node_t {
#ifndef BASELINE
    skey_t key;
    struct node_t<skey_t, sval_t> *volatile left;
    struct node_t<skey_t, sval_t> *volatile right;
    volatile version_t changeOVL;
    struct node_t *volatile parent;
    sval_t value;
    ptlock_t lock; //note: used to be a pointer to a lock!
    volatile int height;

#ifdef PAD_NODES
    char pad[PAD_SIZE];
#endif

#else
    skey_t key;
    sval_t value;
    struct node_t<skey_t, sval_t> * volatile left;
    struct node_t<skey_t, sval_t> * volatile right;
    struct node_t<skey_t, sval_t> * volatile parent;
    unsigned long index;
    long color;
    ptlock_t lock;
    volatile int height;
    volatile version_t changeOVL;
#endif
};

/** This is a special value that indicates the presence of a null value,
 *  to differentiate from the absence of a value.
 */
static void *t_SpecialNull; // reserve an address
static void *SpecialNull = (void *) &t_SpecialNull; // this hack implies sval_t must be a pointer!

/** This is a special value that indicates that an optimistic read
 *  failed.
 */
static void *t_SpecialRetry;
static void *SpecialRetry = (void *) &t_SpecialRetry; // this hack implies sval_t must be a pointer!

/** The number of spins before yielding. */
#define SPIN_COUNT 100

/** The number of yields before blocking. */
#define YIELD_COUNT 0

// we encode directions as characters
#define LEFT 'L'
#define RIGHT 'R'

// return type for extreme searches
#define ReturnKey       0
#define ReturnEntry     1
#define ReturnNode      2

#define OVL_BITS_BEFORE_OVERFLOW 8
#define UnlinkedOVL     (1LL)
#define OVLGrowLockMask (2LL)
#define OVLShrinkLockMask (4LL)
#define OVLGrowCountShift (3)
#define OVLShrinkCountShift (OVLGrowCountShift + OVL_BITS_BEFORE_OVERFLOW)
#define OVLGrowCountMask  (((1L << OVL_BITS_BEFORE_OVERFLOW ) - 1) << OVLGrowCountShift)


#define UpdateAlways        0
#define UpdateIfAbsent      1
#define UpdateIfPresent     2
#define UpdateIfEq          3


#define UnlinkRequired          -1
#define RebalanceRequired       -2
#define NothingRequired         -3

template<typename skey_t, typename sval_t, class RecMgr>
class ccavl {
private:
    PAD;
    RecMgr *const recmgr;
//    PAD;
    node_t<skey_t, sval_t> *root;
//    PAD;
    int init[MAX_THREADS_POW2] = {0,};
//    PAD;

    node_t<skey_t, sval_t> *rb_alloc(const int tid);

    node_t<skey_t, sval_t> *rbnode_create(const int tid, skey_t key, sval_t value, node_t<skey_t, sval_t> *parent);

    sval_t get(const int tid, node_t<skey_t, sval_t> *tree, skey_t key);

    sval_t put(const int tid, node_t<skey_t, sval_t> *tree, skey_t key, sval_t value);

    sval_t putIfAbsent(const int tid, node_t<skey_t, sval_t> *tree, skey_t key, sval_t value);

    sval_t attemptNodeUpdate(
            const int tid,
            int func,
            sval_t expected,
            sval_t newValue,
            node_t<skey_t, sval_t> *parent,
            node_t<skey_t, sval_t> *curr);

    int attemptUnlink_nl(const int tid, node_t<skey_t, sval_t> *parent, node_t<skey_t, sval_t> *curr);

    sval_t remove_node(const int tid, node_t<skey_t, sval_t> *tree, skey_t key);

    int attemptInsertIntoEmpty(const int tid, node_t<skey_t, sval_t> *tree, skey_t key, sval_t vOpt);

    sval_t attemptUpdate(
            const int tid,
            skey_t key,
            int func,
            sval_t expected,
            sval_t newValue,
            node_t<skey_t, sval_t> *parent,
            node_t<skey_t, sval_t> *curr,
            version_t nodeOVL);

    sval_t update(const int tid, node_t<skey_t, sval_t> *tree, skey_t key, int func, sval_t expected, sval_t newValue);

    node_t<skey_t, sval_t> *rebalance_nl(const int tid, node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n);

    void fixHeightAndRebalance(const int tid, node_t<skey_t, sval_t> *curr);

    node_t<skey_t, sval_t> *get_child(node_t<skey_t, sval_t> *curr, char dir);

    void setChild(node_t<skey_t, sval_t> *curr, char dir, node_t<skey_t, sval_t> *new_node);

    void waitUntilChangeCompleted(node_t<skey_t, sval_t> *curr, version_t ovl);

    int height(volatile node_t<skey_t, sval_t> *curr);

    sval_t decodeNull(sval_t v);

    sval_t encodeNull(sval_t v);

    sval_t getImpl(node_t<skey_t, sval_t> *tree, skey_t key);

    sval_t attemptGet(skey_t key,
                      node_t<skey_t, sval_t> *curr,
                      char dirToC,
                      version_t nodeOVL);

    int shouldUpdate(int func, sval_t prev, sval_t expected);

    int nodeCondition(node_t<skey_t, sval_t> *curr);

    node_t<skey_t, sval_t> *fixHeight_nl(node_t<skey_t, sval_t> *curr);

    node_t<skey_t, sval_t> *
    rebalanceToRight_nl(node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n, node_t<skey_t, sval_t> *nL,
                        int hR0);

    node_t<skey_t, sval_t> *
    rebalanceToLeft_nl(node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n, node_t<skey_t, sval_t> *nL, int hR0);

    node_t<skey_t, sval_t> *
    rotateRight_nl(node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n, node_t<skey_t, sval_t> *nL,
                   node_t<skey_t, sval_t> *nLR, int hR, int hLL, int hLR);

    node_t<skey_t, sval_t> *
    rotateLeft_nl(node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n, node_t<skey_t, sval_t> *nR,
                  node_t<skey_t, sval_t> *nRL, int hL, int hRL, int hRR);

    node_t<skey_t, sval_t> *
    rotateLeftOverRight_nl(node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n, node_t<skey_t, sval_t> *nR,
                           node_t<skey_t, sval_t> *nRL, int hL, int hRR, int hRLR);

    node_t<skey_t, sval_t> *
    rotateRightOverLeft_nl(node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n, node_t<skey_t, sval_t> *nL,
                           node_t<skey_t, sval_t> *nLR, int hR, int hLL, int hLRL);

public:
//    PAD;
    const int NUM_PROCESSES;
    skey_t KEY_NEG_INFTY;
    PAD;

    ccavl(const int numProcesses, const skey_t &_KEY_NEG_INFTY)
            : recmgr(new RecMgr(numProcesses, SIGQUIT)), NUM_PROCESSES(numProcesses), KEY_NEG_INFTY(_KEY_NEG_INFTY) {
        const int tid = 0;
        initThread(tid);

        recmgr->endOp(tid);

        root = rbnode_create(tid, KEY_NEG_INFTY, NULL, NULL);
    }

    ~ccavl() {
        delete recmgr;
    }

    void initThread(const int tid) {
        if (init[tid]) return; else init[tid] = !init[tid];

        recmgr->initThread(tid);
    }

    void deinitThread(const int tid) {
        if (!init[tid]) return; else init[tid] = !init[tid];

        recmgr->deinitThread(tid);
    }

    sval_t insertIfAbsent(const int tid, skey_t key, sval_t val) {
        return putIfAbsent(tid, root, key, val);
    }

    sval_t insertReplace(const int tid, skey_t key, sval_t val) {
        return put(tid, root, key, val);
    }

    sval_t find(const int tid, skey_t key) {
        return get(tid, root, key);
    }

    sval_t erase(const int tid, skey_t key) {
        return remove_node(tid, root, key);
    }

    node_t<skey_t, sval_t> *get_root() {
        return root;
    }

    node_t<skey_t, sval_t> *get_left(node_t<skey_t, sval_t> *curr) {
        return curr->left;
    }

    node_t<skey_t, sval_t> *get_right(node_t<skey_t, sval_t> *curr) {
        return curr->right;
    }

    long long getKeyChecksum(node_t<skey_t, sval_t> *curr) {
        if (curr == NULL) return 0;
        node_t<skey_t, sval_t> *left = get_left(curr);
        node_t<skey_t, sval_t> *right = get_right(curr);
        return ((long long) ((curr->value != NULL) ? curr->key : 0))
               + getKeyChecksum(left) + getKeyChecksum(right);
    }

    long long getKeyChecksum() {
        return getKeyChecksum(get_right(root));
    }

    long long getSize(node_t<skey_t, sval_t> *curr) {
        if (curr == NULL) return 0;
        node_t<skey_t, sval_t> *left = get_left(curr);
        node_t<skey_t, sval_t> *right = get_right(curr);
        return (curr->value != NULL) + getSize(left) + getSize(right);
    }

    bool validateStructure() {
        return true;
    }

    long long getSize() {
        return getSize(get_right(root));
    }

    long long getSizeInNodes(node_t<skey_t, sval_t> *const curr) {
        if (curr == NULL) return 0;
        return 1 + getSizeInNodes(get_left(curr)) + getSizeInNodes(get_right(curr));
    }

    long long getSizeInNodes() {
        return getSizeInNodes(root);
    }

    void printSummary() {
        recmgr->printStatus();
    }
};

#endif /* CCAVL_H */

