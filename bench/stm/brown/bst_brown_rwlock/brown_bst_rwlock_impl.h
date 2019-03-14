//
// Created by lwh on 19-3-14.
//

#ifndef BROWN_BST_RWLOCK_H
#define BROWN_BST_RWLOCK_H

#include "brown_bst_rwlock.h"

template<class K, class V, class Compare, class RecManager>
const std::pair<V, bool> bst_rwlock_ns::bst_rwlock<K, V, Compare, RecManager>::find(const int tid, const K &key) {
    auto guard = recmgr->getGuard(tid, true);
    lock.readLock();

    auto p = root->left;
    auto l = p->left;
    if (l == NULL) {
        lock.readUnlock();
        return std::pair<V, bool>(NO_VALUE, false); // no keys in data structure
    }
    while (l->left) {
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    auto result = (key == l->key) ? std::pair<V, bool>(l->value, true) : std::pair<V, bool>(NO_VALUE, false);
    lock.readUnlock();
    return result;
}

template<class K, class V, class Compare, class RecManager>
const V bst_rwlock_ns::bst_rwlock<K, V, Compare, RecManager>::doInsert(const int tid, const K &key, const V &val,
                                                                       bool onlyIfAbsent) {
    auto guard = recmgr->getGuard(tid);
    lock.writeLock();

    auto p = root;
    auto l = p->left;
    while (l->left) {
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    // if we find the key in the tree already
    if (key == l->key) {
        auto result = l->value;
        if (!onlyIfAbsent) {
            l->value = val;
        }
        lock.writeUnlock();
        return result;
    } else {
        auto newLeaf = createNode(tid, key, val, NULL, NULL);
        auto newParent = (l->key == NO_KEY || cmp(key, l->key))
                         ? createNode(tid, l->key, l->value, newLeaf, l)
                         : createNode(tid, key, val, l, newLeaf);

        (l == p->left ? p->left : p->right) = newParent;

        lock.writeUnlock();
        return NO_VALUE;
    }
}

template<class K, class V, class Compare, class RecManager>
const std::pair<V, bool> bst_rwlock_ns::bst_rwlock<K, V, Compare, RecManager>::erase(const int tid, const K &key) {
    auto guard = recmgr->getGuard(tid);
    lock.writeLock();

    auto gp = root;
    auto p = gp->left;
    auto l = p->left;
    if (l == NULL) { // tree is empty
        lock.writeUnlock();
        return std::pair<V, bool>(NO_VALUE, false);
    }
    while (l->left) {
        gp = p;
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    // if we fail to find the key in the tree
    if (key != l->key) {
        lock.writeUnlock();
        return std::pair<V, bool>(NO_VALUE, false);
    } else {
        auto result = l->value;
        auto s = (l == p->left ? p->right : p->left);
        (p == gp->left ? gp->left : gp->right) = s;
        recmgr->retire(tid, p);
        recmgr->retire(tid, l);
        lock.writeUnlock();
        return std::pair<V, bool>(result, true);
    }
}

#endif