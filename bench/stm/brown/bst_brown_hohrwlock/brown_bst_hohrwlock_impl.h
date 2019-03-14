//
// Created by lwh on 19-3-14.
//

#ifndef LOCKFREEC_BROWN_BST_HOHRWLOCK_IMPL_H
#define LOCKFREEC_BROWN_BST_HOHRWLOCK_IMPL_H

#include "brown_bst_hohrwlock.h"

template<class K, class V, class Compare, class RecManager>
const std::pair<V, bool> bst_hohrwlock_ns::bst_hohrwlock<K, V, Compare, RecManager>::find(const int tid, const K &key) {
    auto guard = recmgr->getGuard(tid, true);

    LOCK(root).readLock();
    auto p = root->left;
    LOCK(p).readLock();
    LOCK(root).readUnlock();
    auto l = p->left;
    if (l == NULL) {
        LOCK(p).readUnlock();
        return std::pair<V, bool>(NO_VALUE, false); // no keys in data structure
    }
    while (1) {
        LOCK(l).readLock();
        LOCK(p).readUnlock();
        if (l->left == NULL) break;
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    auto result = (key == l->key) ? std::pair<V, bool>(l->value, true) : std::pair<V, bool>(NO_VALUE, false);
    LOCK(l).readUnlock();
    return result;
}

template<class K, class V, class Compare, class RecManager>
const V bst_hohrwlock_ns::bst_hohrwlock<K, V, Compare, RecManager>::doInsert(const int tid, const K &key, const V &val,
                                                                             bool onlyIfAbsent) {
    auto guard = recmgr->getGuard(tid);
    retry:
    auto p = root;
    LOCK(p).readLock();
    auto l = p->left;
    while (1) {
        LOCK(l).readLock();
        if (l->left == NULL) break;
        LOCK(p).readUnlock(); // want p to be locked after the loop, so release only if we didn't break
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    // now, p and l are read locked

    // if we find the key in the tree already
    if (key == l->key) {
        auto result = l->value;
        if (onlyIfAbsent) {
            LOCK(p).readUnlock();
            LOCK(l).readUnlock();
            return result;
        }

        if (!LOCK(l).upgradeLock()) {
            LOCK(p).readUnlock();
            LOCK(l).readUnlock();
            goto retry;
        }
        // now l is write locked
        l->value = val;

        LOCK(p).readUnlock();
        LOCK(l).writeUnlock();
        return result;

    } else {
        if (!LOCK(p).upgradeLock()) {
            LOCK(p).readUnlock();
            LOCK(l).readUnlock();
            goto retry;
        }
        // now p is write locked

        auto newLeaf = createNode(tid, key, val, NULL, NULL);
        auto newParent = (l->key == NO_KEY || cmp(key, l->key))
                         ? createNode(tid, l->key, l->value, newLeaf, l)
                         : createNode(tid, key, val, l, newLeaf);

        (l == p->left ? p->left : p->right) = newParent;

        LOCK(p).writeUnlock();
        LOCK(l).readUnlock();
        return NO_VALUE;
    }
}

template<class K, class V, class Compare, class RecManager>
const std::pair<V, bool>
bst_hohrwlock_ns::bst_hohrwlock<K, V, Compare, RecManager>::erase(const int tid, const K &key) {
    auto guard = recmgr->getGuard(tid);
    retry:
    auto gp = root;
    LOCK(gp).readLock();
    auto p = gp->left;
    LOCK(p).readLock();
    auto l = p->left;
    if (l == NULL) { // tree is empty
        LOCK(gp).readUnlock();
        LOCK(p).readUnlock();
        return std::pair<V, bool>(NO_VALUE, false);
    }
    // now, gp and p are read locked, and l is non-null

    while (1) {
        LOCK(l).readLock();
        if (l->left == NULL) break;
        LOCK(gp).readUnlock(); // only unlock gp before overwriting the pointer (want gp locked when we exit this loop, since we change it)
        gp = p;
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    // now, gp, p and l are read locked

    // if we fail to find the key in the tree
    if (key != l->key) {
        LOCK(gp).readUnlock();
        LOCK(p).readUnlock();
        LOCK(l).readUnlock();
        return std::pair<V, bool>(NO_VALUE, false);
    } else {
        if (!LOCK(gp).upgradeLock()) {
            LOCK(gp).readUnlock();
            LOCK(p).readUnlock();
            LOCK(l).readUnlock();
            goto retry;
        }
        // now gp is write locked (and p and l are read locked)

        auto result = l->value;
        auto s = (l == p->left ? p->right : p->left);
        (p == gp->left ? gp->left : gp->right) = s;

        recmgr->retire(tid, p);
        recmgr->retire(tid, l);
        LOCK(gp).writeUnlock();
        LOCK(p).readUnlock();
        LOCK(l).readUnlock();
        return std::pair<V, bool>(result, true);
    }
}

#endif //LOCKFREEC_BROWN_BST_HOHRWLOCK_IMPL_H
