/* 
 * File:   intlf.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 7, 2017, 4:00 PM
 */

#ifndef INTLF_H
#define INTLF_H

#include "record_manager.h"

#define LEFT 0
#define RIGHT 1

#define KEY_MASK 0x8000000000000000
#define ADDRESS_MASK 15

#define NULL_BIT 8
#define INJECT_BIT 4
#define DELETE_BIT 2
#define PROMOTE_BIT 1

typedef enum {
    INJECTION, DISCOVERY, CLEANUP
} Mode;

typedef enum {
    SIMPLE, COMPLEX
} Type;

typedef enum {
    DELETE_FLAG, PROMOTE_FLAG
} Flag;

template<typename skey_t, typename sval_t>
struct node_t {
    volatile skey_t markAndKey; //format <markFlag,address>
    node_t<skey_t, sval_t> *volatile child[2]; //format <address,NullBit,InjectFlag,DeleteFlag,PromoteFlag>
    volatile unsigned long readyToReplace;
    sval_t value;
};

template<typename skey_t, typename sval_t>
struct edge {
    node_t<skey_t, sval_t> *parent;
    node_t<skey_t, sval_t> *child;
    int which;
};

template<typename skey_t, typename sval_t>
struct seekRecord {
    edge<skey_t, sval_t> lastEdge;
    edge<skey_t, sval_t> pLastEdge;
    edge<skey_t, sval_t> injectionEdge;
};

template<typename skey_t, typename sval_t>
struct anchorRecord {
    node_t<skey_t, sval_t> *node;
    skey_t key;
};

template<typename skey_t, typename sval_t>
struct stateRecord {
    int depth;
    edge<skey_t, sval_t> targetEdge;
    edge<skey_t, sval_t> pTargetEdge;
    skey_t targetKey;
    skey_t currentKey;
    //sval_t oldValue;
    Mode mode;
    Type type;
    seekRecord<skey_t, sval_t> successorRecord;
};

template<typename skey_t, typename sval_t>
struct tArgs {
    int tid;
    node_t<skey_t, sval_t> *newNode;
    bool isNewNodeAvailable;
    seekRecord<skey_t, sval_t> targetRecord;
    seekRecord<skey_t, sval_t> pSeekRecord;
    stateRecord<skey_t, sval_t> myState;
    struct anchorRecord<skey_t, sval_t> anchorRecord;
    struct anchorRecord<skey_t, sval_t> pAnchorRecord;
};

template<typename skey_t, typename sval_t, class RecMgr>
class intlf {
private:
    PAD;
    const unsigned int idx_id;
    PAD;
    node_t<skey_t, sval_t> *R;
    node_t<skey_t, sval_t> *S;
    node_t<skey_t, sval_t> *T;
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

    void
    populateEdge(edge<skey_t, sval_t> *e, node_t<skey_t, sval_t> *parent, node_t<skey_t, sval_t> *child, int which);

    void copyEdge(edge<skey_t, sval_t> *d, edge<skey_t, sval_t> *s);

    void copySeekRecord(seekRecord<skey_t, sval_t> *d, seekRecord<skey_t, sval_t> *s);

    node_t<skey_t, sval_t> *newLeafNode(tArgs<skey_t, sval_t> *, skey_t key, sval_t value);

    void seek(tArgs<skey_t, sval_t> *, skey_t, seekRecord<skey_t, sval_t> *);

    void initializeTypeAndUpdateMode(tArgs<skey_t, sval_t> *, stateRecord<skey_t, sval_t> *);

    void updateMode(tArgs<skey_t, sval_t> *, stateRecord<skey_t, sval_t> *);

    void inject(tArgs<skey_t, sval_t> *, stateRecord<skey_t, sval_t> *);

    void findSmallest(tArgs<skey_t, sval_t> *, node_t<skey_t, sval_t> *, seekRecord<skey_t, sval_t> *);

    void findAndMarkSuccessor(tArgs<skey_t, sval_t> *, stateRecord<skey_t, sval_t> *);

    void removeSuccessor(tArgs<skey_t, sval_t> *, stateRecord<skey_t, sval_t> *);

    bool cleanup(tArgs<skey_t, sval_t> *, stateRecord<skey_t, sval_t> *);

    bool markChildEdge(tArgs<skey_t, sval_t> *, stateRecord<skey_t, sval_t> *, bool);

    void helpTargetNode(tArgs<skey_t, sval_t> *, edge<skey_t, sval_t> *, int);

    void helpSuccessorNode(tArgs<skey_t, sval_t> *, edge<skey_t, sval_t> *, int);

    node_t<skey_t, sval_t> *simpleSeek(skey_t key, seekRecord<skey_t, sval_t> *s);

    sval_t search(tArgs<skey_t, sval_t> *t, skey_t key);

    sval_t lf_insert(tArgs<skey_t, sval_t> *t, skey_t key, sval_t value);

    bool lf_remove(tArgs<skey_t, sval_t> *t, skey_t key);

public:

    intlf(const int _NUM_THREADS, const skey_t &_KEY_MIN, const skey_t &_KEY_MAX, const sval_t &_VALUE_RESERVED,
          unsigned int id)
            : idx_id(id), NUM_THREADS(_NUM_THREADS), KEY_MIN(_KEY_MIN), KEY_MAX(_KEY_MAX), NO_VALUE(_VALUE_RESERVED),
              recmgr(new RecMgr(NUM_THREADS)) {
        const int tid = 0;
        initThread(tid);

        recmgr->endOp(tid); // enter an initial quiescent state.
        tArgs<skey_t, sval_t> args = {0};
        args.tid = tid;

        R = newLeafNode(&args, KEY_MAX, NO_VALUE);
        R->child[RIGHT] = newLeafNode(&args, KEY_MAX, NO_VALUE);
        S = R->child[RIGHT];
        S->child[RIGHT] = newLeafNode(&args, KEY_MAX, NO_VALUE);
        T = S->child[RIGHT];
    }

    ~intlf() {
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

    node_t<skey_t, sval_t> *get_root() {
        return R;
    }

    RecMgr *debugGetRecMgr() {
        return recmgr;
    }

    sval_t insert(const int tid, skey_t key, sval_t item) {
        assert(key < KEY_MAX);
        tArgs<skey_t, sval_t> args = {0};
        args.tid = tid;
        return lf_insert(&args, key, item);
    }

    bool remove(const int tid, skey_t key) {
        assert(key < KEY_MAX);
        tArgs<skey_t, sval_t> args = {0};
        args.tid = tid;
        return lf_remove(&args, key);
    }

    sval_t find(const int tid, skey_t key) {
        tArgs<skey_t, sval_t> args = {0};
        args.tid = tid;
        return search(&args, key);
    }
};

#endif /* INTLF_H */

