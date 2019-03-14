//
// Created by lwh on 19-3-14.
//

#ifndef LOCKFREEC_BROWN_BST_GLOCK_IMPL_H
#define LOCKFREEC_BROWN_BST_GLOCK_IMPL_H

#include "brown_bst_glock.h"

template<class K, class V, class Compare, class RecManager>
const std::pair<V, bool> bst_glock_ns::bst_glock<K, V, Compare, RecManager>::find(const int tid, const K &key) {
    auto guard = recmgr->getGuard(tid, true);
    acquireLock(&lock);

    auto p = root->left;
    auto l = p->left;
    if (l == NULL) {
        releaseLock(&lock);
        return std::pair<V, bool>(NO_VALUE, false); // no keys in data structure
    }
    while (l->left) {
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    auto result = (key == l->key) ? std::pair<V, bool>(l->value, true) : std::pair<V, bool>(NO_VALUE, false);
    releaseLock(&lock);
    return result;
}

template<class K, class V, class Compare, class RecManager>
const V bst_glock_ns::bst_glock<K, V, Compare, RecManager>::doInsert(const int tid, const K &key, const V &val,
                                                                     bool onlyIfAbsent) {
    auto guard = recmgr->getGuard(tid);
    acquireLock(&lock);

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
        releaseLock(&lock);
        return result;
    } else {
        auto newLeaf = createNode(tid, key, val, NULL, NULL);
        auto newParent = (l->key == NO_KEY || cmp(key, l->key))
                         ? createNode(tid, l->key, l->value, newLeaf, l)
                         : createNode(tid, key, val, l, newLeaf);

        (l == p->left ? p->left : p->right) = newParent;

        releaseLock(&lock);
        return NO_VALUE;
    }
}

template<class K, class V, class Compare, class RecManager>
const std::pair<V, bool> bst_glock_ns::bst_glock<K, V, Compare, RecManager>::erase(const int tid, const K &key) {
    auto guard = recmgr->getGuard(tid);
    acquireLock(&lock);

    auto gp = root;
    auto p = gp->left;
    auto l = p->left;
    if (l == NULL) { // tree is empty
        releaseLock(&lock);
        return std::pair<V, bool>(NO_VALUE, false);
    }
    while (l->left) {
        gp = p;
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    // if we fail to find the key in the tree
    if (key != l->key) {
        releaseLock(&lock);
        return std::pair<V, bool>(NO_VALUE, false);
    } else {
        auto result = l->value;
        auto s = (l == p->left ? p->right : p->left);
        (p == gp->left ? gp->left : gp->right) = s;
        recmgr->retire(tid, p);
        recmgr->retire(tid, l);
        releaseLock(&lock);
        return std::pair<V, bool>(result, true);
    }
}

#endif //LOCKFREEC_BROWN_BST_GLOCK_IMPL_H
