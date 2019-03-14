//
// Created by lwh on 19-3-14.
//

#ifndef CCAVL_IMPL_H
#define CCAVL_IMPL_H

#include "record_manager.h"
#include "ccavl.h"

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ccavl<skey_t, sval_t, RecMgr>::rb_alloc(const int tid) {
    node_t<skey_t, sval_t> *result = recmgr->template allocate<node_t<skey_t, sval_t> >(tid);
    if (result == NULL) {
        setbench_error("out of memory");
    }
    return result;
}

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *
ccavl<skey_t, sval_t, RecMgr>::rbnode_create(const int tid, skey_t key, sval_t value, node_t<skey_t, sval_t> *parent) {
    node_t<skey_t, sval_t> *nnode = rb_alloc(tid);
    nnode->key = key;
    nnode->value = value;
    nnode->right = NULL;
    nnode->left = NULL;
    nnode->parent = parent;
    if (mutex_init(&(nnode->lock)) != 0) {
        printf("\n mutex init failed\n");
    }
    nnode->height = 1;
    nnode->changeOVL = 0;
    return nnode;
}

static int isChanging(version_t ovl) {
    return (ovl & (OVLShrinkLockMask | OVLGrowLockMask)) != 0;
}

static int isUnlinked(version_t ovl) {
    return ovl == UnlinkedOVL;
}

static int isShrinkingOrUnlinked(version_t ovl) {
    return (ovl & (OVLShrinkLockMask | UnlinkedOVL)) != 0;
}

static int isChangingOrUnlinked(version_t ovl) {
    return (ovl & (OVLShrinkLockMask | OVLGrowLockMask | UnlinkedOVL)) != 0;
}

static int hasShrunkOrUnlinked(version_t orig, version_t current) {
    return ((orig ^ current) & ~(OVLGrowLockMask | OVLGrowCountMask)) != 0;
}

/*
 static int hasChangedOrUnlinked(version_t orig, version_t current) {
        return orig != current;
    }
 */
static version_t beginGrow(version_t ovl) {
    assert(!isChangingOrUnlinked(ovl));
    return ovl | OVLGrowLockMask;
}

static version_t endGrow(version_t ovl) {
    assert(!isChangingOrUnlinked(ovl));

    // Overflows will just go into the shrink lock count, which is fine.
    return ovl + (1L << OVLGrowCountShift);
}

static version_t beginShrink(version_t ovl) {
    assert(!isChangingOrUnlinked(ovl));
    return ovl | OVLShrinkLockMask;
}

static version_t endShrink(version_t ovl) {
    assert(!isChangingOrUnlinked(ovl));

    // increment overflows directly
    return ovl + (1L << OVLShrinkCountShift);
}

//***************************************************

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ccavl<skey_t, sval_t, RecMgr>::get_child(node_t<skey_t, sval_t> *curr, char dir) {
    return dir == LEFT ? (node_t<skey_t, sval_t> *) curr->left : (node_t<skey_t, sval_t> *) curr->right;
}


// node should be locked

template<typename skey_t, typename sval_t, class RecMgr>
void ccavl<skey_t, sval_t, RecMgr>::setChild(node_t<skey_t, sval_t> *curr, char dir, node_t<skey_t, sval_t> *new_node) {
    if (dir == LEFT) {
        curr->left = new_node;
    } else {
        curr->right = new_node;
    }
}

//////// per-node blocking

template<typename skey_t, typename sval_t, class RecMgr>
void ccavl<skey_t, sval_t, RecMgr>::waitUntilChangeCompleted(node_t<skey_t, sval_t> *curr, version_t ovl) {
    int tries;

    if (!isChanging(ovl)) {
        return;
    }

    for (tries = 0; tries < SPIN_COUNT; /*++tries*/) {
        if (curr->changeOVL != ovl) {
            return;
        }
    }

    // spin and yield failed, use the nuclear option
    mutex_lock(&(curr->lock));
    // we can't have gotten the lock unless the shrink was over
    mutex_unlock(&(curr->lock));

    assert(curr->changeOVL != ovl);
}

//////// node access functions

template<typename skey_t, typename sval_t, class RecMgr>
int ccavl<skey_t, sval_t, RecMgr>::height(volatile node_t<skey_t, sval_t> *curr) {
    return curr == NULL ? 0 : curr->height;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::decodeNull(sval_t vOpt) {
    assert(vOpt != SpecialRetry);
    return vOpt == SpecialNull ? NULL : vOpt;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::encodeNull(sval_t v) {
    return v == NULL ? SpecialNull : v;
}


//////// search

/** Returns either a value or SpecialNull, if present, or null, if absent. */
template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::getImpl(node_t<skey_t, sval_t> *tree, skey_t key) {
    node_t<skey_t, sval_t> *right;
    version_t ovl;
    //long rightCmp;
    sval_t vo;

    while (1) {
        right = (node_t<skey_t, sval_t> *) tree->right;
        if (right == NULL) {
            return NULL;
        } else {
            //rightCmp = key - right->key;

            if (key == right->key) {
                // who cares how we got here
                return right->value;
            }

            ovl = right->changeOVL;
            if (isShrinkingOrUnlinked(ovl)) {
                waitUntilChangeCompleted(right, ovl);
                // RETRY
            } else if (right == tree->right) {
                // the reread of .right is the one protected by our read of ovl
                vo = attemptGet(key, right, (key < right->key ? LEFT : RIGHT), ovl);
                if (vo != SpecialRetry) {
                    return vo;
                }
                // else RETRY
            }
        }
    }
}


// return a value

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::get(const int tid, node_t<skey_t, sval_t> *tree, skey_t key) {
    auto guard = recmgr->getGuard(tid, true);
    auto retval = decodeNull(getImpl(tree, key));
    return retval;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::attemptGet(skey_t key,
                                                 node_t<skey_t, sval_t> *curr,
                                                 char dirToC,
                                                 version_t nodeOVL) {
    node_t<skey_t, sval_t> *child;
    //long childCmp;
    version_t childOVL;
    sval_t vo;

    while (1) {
        child = get_child(curr, dirToC);

        if (child == NULL) {
            if (hasShrunkOrUnlinked(nodeOVL, curr->changeOVL)) {
                return SpecialRetry;
            }

            // Note is not present.  Read of node.child occurred while
            // parent.child was valid, so we were not affected by any
            // shrinks.
            return NULL;
        } else {
            //childCmp = key - child->key;
            if (key == child->key) {
                // how we got here is irrelevant
                return child->value;
            }

            // child is non-null
            childOVL = child->changeOVL;
            if (isShrinkingOrUnlinked(childOVL)) {
                waitUntilChangeCompleted(child, childOVL);

                if (hasShrunkOrUnlinked(nodeOVL, curr->changeOVL)) {
                    return SpecialRetry;
                }
                // else RETRY
            } else if (child != get_child(curr, dirToC)) {
                // this .child is the one that is protected by childOVL
                if (hasShrunkOrUnlinked(nodeOVL, curr->changeOVL)) {
                    return SpecialRetry;
                }
                // else RETRY
            } else {
                if (hasShrunkOrUnlinked(nodeOVL, curr->changeOVL)) {
                    return SpecialRetry;
                }

                // At this point we know that the traversal our parent took
                // to get to node is still valid.  The recursive
                // implementation will validate the traversal from node to
                // child, so just prior to the nodeOVL validation both
                // traversals were definitely okay.  This means that we are
                // no longer vulnerable to node shrinks, and we don't need
                // to validate nodeOVL any more.
                vo = attemptGet(key, child, (key < child->key ? LEFT : RIGHT), childOVL);
                if (vo != SpecialRetry) {
                    return vo;
                }
                // else RETRY
            }
        }
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
int ccavl<skey_t, sval_t, RecMgr>::shouldUpdate(int func, sval_t prev, sval_t expected) {
    switch (func) {
        case UpdateAlways:
            return 1;
        case UpdateIfAbsent:
            return prev == NULL;
        case UpdateIfPresent:
            return prev != NULL;
        default:
            return prev == expected; // TODO: use .equals
    }
}

// return previous value or NULL

template<typename skey_t, typename sval_t, class RecMgr>
sval_t
ccavl<skey_t, sval_t, RecMgr>::putIfAbsent(const int tid, node_t<skey_t, sval_t> *tree, skey_t key, sval_t value) {
    auto guard = recmgr->getGuard(tid);
    auto retval = decodeNull(update(tid, tree, key, UpdateIfAbsent, NULL, encodeNull(value)));
    return retval;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::put(const int tid, node_t<skey_t, sval_t> *tree, skey_t key, sval_t value) {
    auto guard = recmgr->getGuard(tid);
    auto retval = decodeNull(update(tid, tree, key, UpdateAlways, NULL, encodeNull(value)));
    return retval;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::remove_node(const int tid, node_t<skey_t, sval_t> *tree, skey_t key) {
    auto guard = recmgr->getGuard(tid);
    auto retval = decodeNull(update(tid, tree, key, UpdateAlways, NULL, NULL));
    return retval;
}

template<typename skey_t, typename sval_t, class RecMgr>
int ccavl<skey_t, sval_t, RecMgr>::attemptInsertIntoEmpty(const int tid, node_t<skey_t, sval_t> *tree, skey_t key,
                                                          sval_t vOpt) {
    mutex_lock(&(tree->lock));
    if (tree->right == NULL) {
        tree->right = rbnode_create(tid, key, vOpt, tree);
        tree->height = 2;
        mutex_unlock(&(tree->lock));
        return 1;
    } else {
        mutex_unlock(&(tree->lock));
        return 0;
    }
}

/** If successful returns the non-null previous value, SpecialNull for a
 *  null previous value, or null if not previously in the map.
 *  The caller should retry if this method returns SpecialRetry.
 */
template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::attemptUpdate(
        const int tid,
        skey_t key,
        int func,
        sval_t expected,
        sval_t newValue,
        node_t<skey_t, sval_t> *parent,
        node_t<skey_t, sval_t> *curr,
        version_t nodeOVL) {
    // As the search progresses there is an implicit min and max assumed for the
    // branch of the tree rooted at node. A left rotation of a node x results in
    // the range of keys in the right branch of x being reduced, so if we are at a
    // node and we wish to traverse to one of the branches we must make sure that
    // the node has not undergone a rotation since arriving from the parent.
    //
    // A rotation of node can't screw us up once we have traversed to node's
    // child, so we don't need to build a huge transaction, just a chain of
    // smaller read-only transactions.
    //long cmp;
    char dirToC;

    assert(parent != curr);
    assert(nodeOVL != UnlinkedOVL);

    //cmp = key - curr->key;
    if (key == curr->key) {
        return attemptNodeUpdate(tid, func, expected, newValue, parent, curr);
    }

    dirToC = key < curr->key ? LEFT : RIGHT;

    while (1) {
        node_t<skey_t, sval_t> *child = get_child(curr, dirToC);

        if (hasShrunkOrUnlinked(nodeOVL, curr->changeOVL)) {
            return SpecialRetry;
        }

        if (child == NULL) {
            // key is not present
            if (newValue == NULL) {
                // Removal is requested.  Read of node.child occurred
                // while parent.child was valid, so we were not affected
                // by any shrinks.
                return NULL;
            } else {
                // Update will be an insert.
                int success;
                node_t<skey_t, sval_t> *damaged;
                mutex_lock(&(curr->lock));
                {
                    // Validate that we haven't been affected by past
                    // rotations.  We've got the lock on node, so no future
                    // rotations can mess with us.
                    if (hasShrunkOrUnlinked(nodeOVL, curr->changeOVL)) {
                        mutex_unlock(&(curr->lock));
                        return SpecialRetry;
                    }

                    if (get_child(curr, dirToC) != NULL) {
                        // Lost a race with a concurrent insert.  No need
                        // to back up to the parent, but we must RETRY in
                        // the outer loop of this method.
                        success = 0;
                        damaged = NULL;
                    } else {
                        // We're valid.  Does the user still want to
                        // perform the operation?
                        if (!shouldUpdate(func, NULL, expected)) {
                            mutex_unlock(&(curr->lock));
                            return NULL;
                        }

                        // Create a new leaf
                        setChild(curr, dirToC, rbnode_create(tid, key, newValue, curr));
                        success = 1;

                        // attempt to fix node.height while we've still got
                        // the lock
                        damaged = fixHeight_nl(curr);
                    }
                }
                mutex_unlock(&(curr->lock));
                if (success) {
                    fixHeightAndRebalance(tid, damaged);
                    return NULL;
                }
                // else RETRY
            }
        } else {
            // non-null child
            version_t childOVL = child->changeOVL;
            if (isShrinkingOrUnlinked(childOVL)) {
                waitUntilChangeCompleted(child, childOVL);
                // RETRY
            } else if (child != get_child(curr, dirToC)) {
                // this second read is important, because it is protected
                // by childOVL
                // RETRY
            } else {
                // validate the read that our caller took to get to node
                if (hasShrunkOrUnlinked(nodeOVL, curr->changeOVL)) {
                    return SpecialRetry;
                }

                // At this point we know that the traversal our parent took
                // to get to node is still valid.  The recursive
                // implementation will validate the traversal from node to
                // child, so just prior to the nodeOVL validation both
                // traversals were definitely okay.  This means that we are
                // no longer vulnerable to node shrinks, and we don't need
                // to validate nodeOVL any more.
                sval_t vo = attemptUpdate(tid, key, func,
                                          expected, newValue, curr, child, childOVL);
                if (vo != SpecialRetry) {
                    return vo;
                }
                // else RETRY
            }
        }
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::update(const int tid, node_t<skey_t, sval_t> *tree, skey_t key, int func,
                                             sval_t expected, sval_t newValue) {

    while (1) {
        node_t<skey_t, sval_t> *right = (node_t<skey_t, sval_t> *) tree->right;
        if (right == NULL) {
            // key is not present
            if (!shouldUpdate(func, NULL, expected) ||
                newValue == NULL ||
                attemptInsertIntoEmpty(tid, tree, key, newValue)) {
                // nothing needs to be done, or we were successful, prev value is Absent
                return NULL;
            }
            // else RETRY
        } else {
            version_t ovl = right->changeOVL;
            if (isShrinkingOrUnlinked(ovl)) {
                waitUntilChangeCompleted(right, ovl);
                // RETRY
            } else if (right == tree->right) {
                // this is the protected .right
                sval_t vo = attemptUpdate(tid, key, func,
                                          expected, newValue, tree, right, ovl);
                if (vo != SpecialRetry) {
                    return vo;
                }
                // else RETRY
            }
        }
    }
}

/** parent will only be used for unlink, update can proceed even if parent
 *  is stale.
 */
template<typename skey_t, typename sval_t, class RecMgr>
sval_t ccavl<skey_t, sval_t, RecMgr>::attemptNodeUpdate(
        const int tid,
        int func,
        sval_t expected,
        sval_t newValue,
        node_t<skey_t, sval_t> *parent,
        node_t<skey_t, sval_t> *curr) {
    sval_t prev;

    if (newValue == NULL) {
        // removal
        if (curr->value == NULL) {
            // This node is already removed, nothing to do.
            return NULL;
        }
    }

    if (newValue == NULL && (curr->left == NULL || curr->right == NULL)) {
        // potential unlink, get ready by locking the parent
        node_t<skey_t, sval_t> *damaged;
        mutex_lock(&(parent->lock));
        {
            if (isUnlinked(parent->changeOVL) || curr->parent != parent) {
                mutex_unlock(&(parent->lock));
                return SpecialRetry;
            }

            mutex_lock(&(curr->lock));
            {
                prev = curr->value;
                if (prev == NULL || !shouldUpdate(func, prev, expected)) {
                    // nothing to do
                    mutex_unlock(&(curr->lock));
                    mutex_unlock(&(parent->lock));
                    return prev;
                }
                if (!attemptUnlink_nl(tid, parent, curr)) {
                    mutex_unlock(&(curr->lock));
                    mutex_unlock(&(parent->lock));
                    return SpecialRetry;
                }
            }
            mutex_unlock(&(curr->lock));

            // try to fix the parent while we've still got the lock
            damaged = fixHeight_nl(parent);
        }
        mutex_unlock(&(parent->lock));
        fixHeightAndRebalance(tid, damaged);
        return prev;
    } else {
        // potential update (including remove-without-unlink)
        mutex_lock(&(curr->lock));
        {
            // regular version changes don't bother us
            if (isUnlinked(curr->changeOVL)) {
                mutex_unlock(&(curr->lock));
                return SpecialRetry;
            }

            prev = curr->value;
            if (!shouldUpdate(func, prev, expected)) {
                mutex_unlock(&(curr->lock));
                return prev;
            }

            // retry if we now detect that unlink is possible
            if (newValue == NULL && (curr->left == NULL || curr->right == NULL)) {
                mutex_unlock(&(curr->lock));
                return SpecialRetry;
            }

            // update in-place
            curr->value = newValue;
            mutex_unlock(&(curr->lock));
            return prev;
        }
        mutex_unlock(&(curr->lock));
    }
}

/** Does not adjust the size or any heights. */
template<typename skey_t, typename sval_t, class RecMgr>
int ccavl<skey_t, sval_t, RecMgr>::attemptUnlink_nl(const int tid, node_t<skey_t, sval_t> *parent,
                                                    node_t<skey_t, sval_t> *curr) {
    node_t<skey_t, sval_t> *parentL;
    node_t<skey_t, sval_t> *parentR;
    node_t<skey_t, sval_t> *left;
    node_t<skey_t, sval_t> *right;
    node_t<skey_t, sval_t> *splice;

    // assert (Thread.holdsLock(parent));
    // assert (Thread.holdsLock( node_t<skey_t, sval_t>*));
    assert(!isUnlinked(parent->changeOVL));

    parentL = (node_t<skey_t, sval_t> *) parent->left;
    parentR = (node_t<skey_t, sval_t> *) parent->right;
    if (parentL != curr && parentR != curr) {
        // node is no longer a child of parent
        return 0;
    }

    assert(!isUnlinked(curr->changeOVL));
    assert(parent == curr->parent);

    left = (node_t<skey_t, sval_t> *) curr->left;
    right = (node_t<skey_t, sval_t> *) curr->right;
    if (left != NULL && right != NULL) {
        // splicing is no longer possible
        return 0;
    }
    splice = left != NULL ? left : right;

    assert(splice != curr);

    if (parentL == curr) {
        parent->left = splice;
    } else {
        parent->right = splice;
    }
    recmgr->retire(tid, curr);
    if (splice != NULL) {
        mutex_lock(&(splice->lock));
        splice->parent = parent;
        mutex_unlock(&(splice->lock));
    }

    lock_mb();
    curr->changeOVL = UnlinkedOVL;
    curr->value = NULL;
    lock_mb();
    //printf("unlink %p %p %p\n", parent, node, splice);
    // NOTE: this is a hack to allow deeply nested routines to be able to
    //       see the root of the tree. This is necessary to allow rp_free
    //       to be passed the tree lock rather than the node lock.
    //       My_Tree is a thread local variable that is set by the
    //       public interface on each method call
    //
    //rp_free(My_Tree->lock, rbnode_free, node);
    // FIX THIS: not doing garbage collection

    return 1;
}

//////////////// tree balance and height info repair

template<typename skey_t, typename sval_t, class RecMgr>
int ccavl<skey_t, sval_t, RecMgr>::nodeCondition(node_t<skey_t, sval_t> *curr) {
    // Begin atomic.

    int hN;
    int hL0;
    int hR0;
    int hNRepl;
    int bal;
    node_t<skey_t, sval_t> *nL = (node_t<skey_t, sval_t> *) curr->left;
    node_t<skey_t, sval_t> *nR = (node_t<skey_t, sval_t> *) curr->right;

    if ((nL == NULL || nR == NULL) && curr->value == NULL) {
        return UnlinkRequired;
    }

    hN = curr->height;
    hL0 = height(nL);
    hR0 = height(nR);

    // End atomic.  Since any thread that changes a node promises to fix
    // it, either our read was consistent (and a NothingRequired conclusion
    // is correct) or someone else has taken responsibility for either node
    // or one of its children.

    hNRepl = 1 + MAX(hL0, hR0);
    bal = hL0 - hR0;

    if (bal < -1 || bal > 1) {
        return RebalanceRequired;
    }

    return hN != hNRepl ? hNRepl : NothingRequired;
}

template<typename skey_t, typename sval_t, class RecMgr>
void ccavl<skey_t, sval_t, RecMgr>::fixHeightAndRebalance(const int tid, node_t<skey_t, sval_t> *curr) {
    while (curr != NULL && curr->parent != NULL) {
        int condition = nodeCondition(curr);
        if (condition == NothingRequired || isUnlinked(curr->changeOVL)) {
            // nothing to do, or no point in fixing this node
            return;
        }

        if (condition != UnlinkRequired && condition != RebalanceRequired) {
            node_t<skey_t, sval_t> *new_node;
            mutex_lock(&(curr->lock));
            {
                new_node = fixHeight_nl(curr);
            }
            mutex_unlock(&(curr->lock));
            curr = new_node;
        } else {
            node_t<skey_t, sval_t> *nParent = (node_t<skey_t, sval_t> *) curr->parent;
            mutex_lock(&(nParent->lock));
            {
                if (!isUnlinked(nParent->changeOVL) && curr->parent == nParent) {
                    node_t<skey_t, sval_t> *new_node;
                    mutex_lock(&(curr->lock));
                    {
                        new_node = rebalance_nl(tid, nParent, curr);
                    }
                    mutex_unlock(&(curr->lock));
                    curr = new_node;
                }
                // else RETRY
            }
            mutex_unlock(&(nParent->lock));
        }
    }
}

/** Attempts to fix the height of a (locked) damaged node, returning the
 *  lowest damaged node for which this thread is responsible.  Returns null
 *  if no more repairs are needed.
 */
template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ccavl<skey_t, sval_t, RecMgr>::fixHeight_nl(node_t<skey_t, sval_t> *curr) {
    int c = nodeCondition(curr);
    switch (c) {
        case RebalanceRequired:
        case UnlinkRequired:
            // can't repair
            return curr;
        case NothingRequired:
            // Any future damage to this node is not our responsibility.
            return NULL;
        default:
            curr->height = c;
            // we've damaged our parent, but we can't fix it now
            return (node_t<skey_t, sval_t> *) curr->parent;
    }
}

/** nParent and n must be locked on entry.  Returns a damaged node, or null
 *  if no more rebalancing is necessary.
 */
template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *
ccavl<skey_t, sval_t, RecMgr>::rebalance_nl(const int tid, node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n) {
    int hN;
    int hL0;
    int hR0;
    int hNRepl;
    int bal;

    node_t<skey_t, sval_t> *tainted;
    node_t<skey_t, sval_t> *nL = (node_t<skey_t, sval_t> *) n->left;
    node_t<skey_t, sval_t> *nR = (node_t<skey_t, sval_t> *) n->right;

    if ((nL == NULL || nR == NULL) && n->value == NULL) {
        if (attemptUnlink_nl(tid, nParent, n)) {
            // attempt to fix nParent.height while we've still got the lock
            return fixHeight_nl(nParent);
        } else {
            // retry needed for n
            return n;
        }
    }

    hN = n->height;
    hL0 = height(nL);
    hR0 = height(nR);
    hNRepl = 1 + MAX(hL0, hR0);
    bal = hL0 - hR0;

    if (bal > 1) {
        mutex_lock(&(nL->lock));
        tainted = rebalanceToRight_nl(nParent, n, nL, hR0);
        mutex_unlock(&(nL->lock));
        return tainted;
    } else if (bal < -1) {
        mutex_lock(&(nR->lock));
        tainted = rebalanceToLeft_nl(nParent, n, nR, hL0);
        mutex_unlock(&(nR->lock));
        return tainted;
    } else if (hNRepl != hN) {
        // we've got more than enough locks to do a height change, no need to
        // trigger a retry
        n->height = hNRepl;

        // nParent is already locked, let's try to fix it too
        return fixHeight_nl(nParent);
    } else {
        // nothing to do
        return NULL;
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *
ccavl<skey_t, sval_t, RecMgr>::rebalanceToRight_nl(node_t<skey_t, sval_t> *nParent, node_t<skey_t, sval_t> *n,
                                                   node_t<skey_t, sval_t> *nL, int hR0) {
    node_t<skey_t, sval_t> *result;

    // L is too large, we will rotate-right.  If L.R is taller
    // than L.L, then we will first rotate-left L.
    {
        int hL = nL->height;
        if (hL - hR0 <= 1) {
            return n; // retry
        } else {
            node_t<skey_t, sval_t> *nLR = (node_t<skey_t, sval_t> *) nL->right;
            int hLL0 = height((node_t<skey_t, sval_t> *) nL->left);
            int hLR0 = height(nLR);
            if (hLL0 >= hLR0) {
                // rotate right based on our snapshot of hLR
                if (nLR != NULL) mutex_lock(&(nLR->lock));
                result = rotateRight_nl(nParent, n, nL, nLR, hR0, hLL0, hLR0);
                if (nLR != NULL) mutex_unlock(&(nLR->lock));
                return result;
            } else {
                mutex_lock(&(nLR->lock));
                {
                    // If our hLR snapshot is incorrect then we might
                    // actually need to do a single rotate-right on n.
                    int hLR = nLR->height;
                    if (hLL0 >= hLR) {
                        result = rotateRight_nl(nParent, n, nL, nLR, hR0, hLL0, hLR);
                        mutex_unlock(&(nLR->lock));
                        return result;
                    } else {
                        // If the underlying left balance would not be
                        // sufficient to actually fix n.left, then instead
                        // of rolling it into a double rotation we do it on
                        // it's own.  This may let us avoid rotating n at
                        // all, but more importantly it avoids the creation
                        // of damaged nodes that don't have a direct
                        // ancestry relationship.  The recursive call to
                        // rebalanceToRight_nl in this case occurs after we
                        // release the lock on nLR.
                        int hLRL = height((node_t<skey_t, sval_t> *) nLR->left);
                        int b = hLL0 - hLRL;
                        if (b >= -1 && b <= 1) {
                            // nParent.child.left won't be damaged after a double rotation
                            result = rotateRightOverLeft_nl(nParent, n, nL, nLR,
                                                            hR0, hLL0, hLRL);
                            mutex_unlock(&(nLR->lock));
                            return result;
                        }
                    }
                }
                // focus on nL, if necessary n will be balanced later
                result = rebalanceToLeft_nl(n, nL, nLR, hLL0);
                mutex_unlock(&(nLR->lock));
                return result;
            }
        }
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ccavl<skey_t, sval_t, RecMgr>::rebalanceToLeft_nl(node_t<skey_t, sval_t> *nParent,
                                                                          node_t<skey_t, sval_t> *n,
                                                                          node_t<skey_t, sval_t> *nR,
                                                                          int hL0) {
    node_t<skey_t, sval_t> *result;

    {
        int hR = nR->height;
        if (hL0 - hR >= -1) {
            return n; // retry
        } else {
            node_t<skey_t, sval_t> *nRL = (node_t<skey_t, sval_t> *) nR->left;
            int hRL0 = height(nRL);
            int hRR0 = height((node_t<skey_t, sval_t> *) nR->right);
            if (hRR0 >= hRL0) {
                if (nRL != NULL) mutex_lock(&(nRL->lock));
                result = rotateLeft_nl(nParent, n, nR, nRL, hL0, hRL0, hRR0);
                if (nRL != NULL) mutex_unlock(&(nRL->lock));
                return result;
            } else {
                mutex_lock(&(nRL->lock));
                {
                    int hRL = nRL->height;
                    if (hRR0 >= hRL) {
                        result = rotateLeft_nl(nParent, n, nR, nRL, hL0, hRL, hRR0);
                        mutex_unlock(&(nRL->lock));
                        return result;
                    } else {
                        int hRLR = height((node_t<skey_t, sval_t> *) nRL->right);
                        int b = hRR0 - hRLR;
                        if (b >= -1 && b <= 1) {
                            result = rotateLeftOverRight_nl(nParent, n,
                                                            nR, nRL, hL0, hRR0, hRLR);
                            mutex_unlock(&(nRL->lock));
                            return result;
                        }
                    }
                }
                result = rebalanceToRight_nl(n, nR, nRL, hRR0);
                mutex_unlock(&(nRL->lock));
                return result;
            }
        }
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ccavl<skey_t, sval_t, RecMgr>::rotateRight_nl(node_t<skey_t, sval_t> *nParent,
                                                                      node_t<skey_t, sval_t> *n,
                                                                      node_t<skey_t, sval_t> *nL,
                                                                      node_t<skey_t, sval_t> *nLR,
                                                                      int hR,
                                                                      int hLL,
                                                                      int hLR) {
    int hNRepl;
    int balN;
    int balL;
    version_t nodeOVL = n->changeOVL;
    version_t leftOVL = nL->changeOVL;

    node_t<skey_t, sval_t> *nPL = (node_t<skey_t, sval_t> *) nParent->left;

    n->changeOVL = beginShrink(nodeOVL);
    nL->changeOVL = beginGrow(leftOVL);
    lock_mb();

    // Down links originally to shrinking nodes should be the last to change,
    // because if we change them early a search might bypass the OVL that
    // indicates its invalidity.  Down links originally from shrinking nodes
    // should be the first to change, because we have complete freedom when to
    // change them.  s/down/up/ and s/shrink/grow/ for the parent links.

    n->left = nLR;
    nL->right = n;
    if (nPL == n) {
        nParent->left = nL;
    } else {
        nParent->right = nL;
    }

    nL->parent = nParent;
    n->parent = nL;
    if (nLR != NULL) {
        nLR->parent = n;
    }

    // fix up heights links
    hNRepl = 1 + MAX(hLR, hR);
    n->height = hNRepl;
    nL->height = 1 + MAX(hLL, hNRepl);

    nL->changeOVL = endGrow(leftOVL);
    n->changeOVL = endShrink(nodeOVL);
    lock_mb();

    // We have damaged nParent, n (now parent.child.right), and nL (now
    // parent.child).  n is the deepest.  Perform as many fixes as we can
    // with the locks we've got.

    // We've already fixed the height for n, but it might still be outside
    // our allowable balance range.  In that case a simple fixHeight_nl
    // won't help.
    balN = hLR - hR;
    if (balN < -1 || balN > 1) {
        // we need another rotation at n
        return n;
    }

    // we've already fixed the height at nL, do we need a rotation here?
    balL = hLL - hNRepl;
    if (balL < -1 || balL > 1) {
        return nL;
    }

    // try to fix the parent height while we've still got the lock
    return fixHeight_nl(nParent);
}

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ccavl<skey_t, sval_t, RecMgr>::rotateLeft_nl(node_t<skey_t, sval_t> *nParent,
                                                                     node_t<skey_t, sval_t> *n,
                                                                     node_t<skey_t, sval_t> *nR,
                                                                     node_t<skey_t, sval_t> *nRL,
                                                                     int hL,
                                                                     int hRL,
                                                                     int hRR) {
    int hNRepl;
    int balN;
    int balR;
    version_t nodeOVL = n->changeOVL;
    version_t rightOVL = nR->changeOVL;

    node_t<skey_t, sval_t> *nPL = (node_t<skey_t, sval_t> *) nParent->left;

    n->changeOVL = beginShrink(nodeOVL);
    nR->changeOVL = beginGrow(rightOVL);
    lock_mb();

    n->right = nRL;
    nR->left = n;
    if (nPL == n) {
        nParent->left = nR;
    } else {
        nParent->right = nR;
    }

    nR->parent = nParent;
    n->parent = nR;
    if (nRL != NULL) {
        nRL->parent = n;
    }

    // fix up heights
    hNRepl = 1 + MAX(hL, hRL);
    n->height = hNRepl;
    nR->height = 1 + MAX(hNRepl, hRR);

    nR->changeOVL = endGrow(rightOVL);
    n->changeOVL = endShrink(nodeOVL);
    lock_mb();

    balN = hRL - hL;
    if (balN < -1 || balN > 1) {
        return n;
    }

    balR = hRR - hNRepl;
    if (balR < -1 || balR > 1) {
        return nR;
    }

    return fixHeight_nl(nParent);
}

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ccavl<skey_t, sval_t, RecMgr>::rotateRightOverLeft_nl(node_t<skey_t, sval_t> *nParent,
                                                                              node_t<skey_t, sval_t> *n,
                                                                              node_t<skey_t, sval_t> *nL,
                                                                              node_t<skey_t, sval_t> *nLR,
                                                                              int hR,
                                                                              int hLL,
                                                                              int hLRL) {
    int hNRepl;
    int hLRepl;
    int balN;
    int balLR;

    version_t nodeOVL = n->changeOVL;
    version_t leftOVL = nL->changeOVL;
    version_t leftROVL = nLR->changeOVL;

    node_t<skey_t, sval_t> *nPL = (node_t<skey_t, sval_t> *) nParent->left;
    node_t<skey_t, sval_t> *nLRL = (node_t<skey_t, sval_t> *) nLR->left;
    node_t<skey_t, sval_t> *nLRR = (node_t<skey_t, sval_t> *) nLR->right;
    int hLRR = height(nLRR);

    n->changeOVL = beginShrink(nodeOVL);
    nL->changeOVL = beginShrink(leftOVL);
    nLR->changeOVL = beginGrow(leftROVL);
    lock_mb();

    n->left = nLRR;
    nL->right = nLRL;
    nLR->left = nL;
    nLR->right = n;
    if (nPL == n) {
        nParent->left = nLR;
    } else {
        nParent->right = nLR;
    }

    nLR->parent = nParent;
    nL->parent = nLR;
    n->parent = nLR;
    if (nLRR != NULL) {
        nLRR->parent = n;
    }
    if (nLRL != NULL) {
        nLRL->parent = nL;
    }

    // fix up heights
    hNRepl = 1 + MAX(hLRR, hR);
    n->height = hNRepl;
    hLRepl = 1 + MAX(hLL, hLRL);
    nL->height = hLRepl;
    nLR->height = 1 + MAX(hLRepl, hNRepl);

    nLR->changeOVL = endGrow(leftROVL);
    nL->changeOVL = endShrink(leftOVL);
    n->changeOVL = endShrink(nodeOVL);
    lock_mb();

    // caller should have performed only a single rotation if nL was going
    // to end up damaged
    assert(ABS(hLL - hLRL) <= 1);

    // We have damaged nParent, nLR (now parent.child), and n (now
    // parent.child.right).  n is the deepest.  Perform as many fixes as we
    // can with the locks we've got.

    // We've already fixed the height for n, but it might still be outside
    // our allowable balance range.  In that case a simple fixHeight_nl
    // won't help.
    balN = hLRR - hR;
    if (balN < -1 || balN > 1) {
        // we need another rotation at n
        return n;
    }

    // we've already fixed the height at nLR, do we need a rotation here?
    balLR = hLRepl - hNRepl;
    if (balLR < -1 || balLR > 1) {
        return nLR;
    }

    // try to fix the parent height while we've still got the lock
    return fixHeight_nl(nParent);
}

template<typename skey_t, typename sval_t, class RecMgr>
node_t<skey_t, sval_t> *ccavl<skey_t, sval_t, RecMgr>::rotateLeftOverRight_nl(node_t<skey_t, sval_t> *nParent,
                                                                              node_t<skey_t, sval_t> *n,
                                                                              node_t<skey_t, sval_t> *nR,
                                                                              node_t<skey_t, sval_t> *nRL,
                                                                              int hL,
                                                                              int hRR,
                                                                              int hRLR) {
    int hNRepl;
    int hRRepl;
    int balN;
    int balRL;

    version_t nodeOVL = n->changeOVL;
    version_t rightOVL = nR->changeOVL;
    version_t rightLOVL = nRL->changeOVL;

    node_t<skey_t, sval_t> *nPL = (node_t<skey_t, sval_t> *) nParent->left;
    node_t<skey_t, sval_t> *nRLL = (node_t<skey_t, sval_t> *) nRL->left;
    int hRLL = height(nRLL);
    node_t<skey_t, sval_t> *nRLR = (node_t<skey_t, sval_t> *) nRL->right;

    n->changeOVL = beginShrink(nodeOVL);
    nR->changeOVL = beginShrink(rightOVL);
    nRL->changeOVL = beginGrow(rightLOVL);
    lock_mb();

    n->right = nRLL;
    nR->left = nRLR;
    nRL->right = nR;
    nRL->left = n;
    if (nPL == n) {
        nParent->left = nRL;
    } else {
        nParent->right = nRL;
    }

    nRL->parent = nParent;
    nR->parent = nRL;
    n->parent = nRL;
    if (nRLL != NULL) {
        nRLL->parent = n;
    }
    if (nRLR != NULL) {
        nRLR->parent = nR;
    }

    // fix up heights
    hNRepl = 1 + MAX(hL, hRLL);
    n->height = hNRepl;
    hRRepl = 1 + MAX(hRLR, hRR);
    nR->height = hRRepl;
    nRL->height = 1 + MAX(hNRepl, hRRepl);

    nRL->changeOVL = endGrow(rightLOVL);
    nR->changeOVL = endShrink(rightOVL);
    n->changeOVL = endShrink(nodeOVL);
    lock_mb();

    assert(ABS(hRR - hRLR) <= 1);

    balN = hRLL - hL;
    if (balN < -1 || balN > 1) {
        return n;
    }
    balRL = hRRepl - hNRepl;
    if (balRL < -1 || balRL > 1) {
        return nRL;
    }
    return fixHeight_nl(nParent);
}

#endif /* CCAVL_IMPL_H */