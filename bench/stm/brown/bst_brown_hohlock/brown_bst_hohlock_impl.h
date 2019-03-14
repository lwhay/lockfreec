//
// Created by lwh on 19-3-14.
//

#ifndef LOCKFREEC_BROWN_BST_HOHLOCK_IMPL_H
#define LOCKFREEC_BROWN_BST_HOHLOCK_IMPL_H

#include "brown_bst_hohlock.h"

template<class K, class V, class Compare, class RecManager>
const std::pair<V, bool> bst_hohlock_ns::bst_hohlock<K, V, Compare, RecManager>::find(const int tid, const K &key) {
    auto guard = recmgr->getGuard(tid, true);

    acquireLock(&root->lock);
    auto p = root->left;
    acquireLock(&p->lock);
    releaseLock(&root->lock);
    auto l = p->left;
    if (l == NULL) {
        releaseLock(&p->lock);
        return std::pair<V, bool>(NO_VALUE, false); // no keys in data structure
    }
    while (1) {
        acquireLock(&l->lock);
        releaseLock(&p->lock);
        if (l->left == NULL) break;
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    auto result = (key == l->key) ? std::pair<V, bool>(l->value, true) : std::pair<V, bool>(NO_VALUE, false);
    releaseLock(&l->lock);
    return result;
}

template<class K, class V, class Compare, class RecManager>
const V bst_hohlock_ns::bst_hohlock<K, V, Compare, RecManager>::doInsert(const int tid, const K &key, const V &val,
                                                                         bool onlyIfAbsent) {
    auto guard = recmgr->getGuard(tid);

    auto p = root;
    acquireLock(&p->lock);
    auto l = p->left;
    while (1) {
        acquireLock(&l->lock);
        if (l->left == NULL) break;
        releaseLock(&p->lock); // want p to be locked after the loop, so release only if we didn't break
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    // now, p and l are locked

    // if we find the key in the tree already
    if (key == l->key) {
        auto result = l->value;
        if (!onlyIfAbsent) {
            l->value = val;
        }
        releaseLock(&p->lock);
        releaseLock(&l->lock);
        return result;
    } else {
        auto newLeaf = createNode(tid, key, val, NULL, NULL);
        auto newParent = (l->key == NO_KEY || cmp(key, l->key))
                         ? createNode(tid, l->key, l->value, newLeaf, l)
                         : createNode(tid, key, val, l, newLeaf);

        (l == p->left ? p->left : p->right) = newParent;

        releaseLock(&p->lock);
        releaseLock(&l->lock);
        return NO_VALUE;
    }
}

template<class K, class V, class Compare, class RecManager>
const std::pair<V, bool> bst_hohlock_ns::bst_hohlock<K, V, Compare, RecManager>::erase(const int tid, const K &key) {
    auto guard = recmgr->getGuard(tid);

    auto gp = root;
    acquireLock(&gp->lock);
    auto p = gp->left;
    acquireLock(&p->lock);
    auto l = p->left;
    if (l == NULL) { // tree is empty
        releaseLock(&gp->lock);
        releaseLock(&p->lock);
        return std::pair<V, bool>(NO_VALUE, false);
    }
    // now, gp and p are locked, and l is non-null

    while (1) {
        acquireLock(&l->lock);
        if (l->left == NULL) break;
        releaseLock(
                &gp->lock); // only release just before overwriting the local gp pointer, since we want gp to be locked after the loop, when we modify it below
        gp = p;
        p = l;
        l = (p->key == NO_KEY || cmp(key, p->key)) ? p->left : p->right;
    }
    // now, gp, p and l are all locked

    // if we fail to find the key in the tree
    if (key != l->key) {
        releaseLock(&gp->lock);
        releaseLock(&p->lock);
        releaseLock(&l->lock);
        return std::pair<V, bool>(NO_VALUE, false);
    } else {
        auto result = l->value;
        auto s = (l == p->left ? p->right : p->left);
        (p == gp->left ? gp->left : gp->right) = s;
        recmgr->retire(tid, p);
        recmgr->retire(tid, l);
        releaseLock(&gp->lock);
        releaseLock(&p->lock);
        releaseLock(&l->lock);
        return std::pair<V, bool>(result, true);
    }
}

#endif //LOCKFREEC_BROWN_BST_HOHLOCK_IMPL_H
