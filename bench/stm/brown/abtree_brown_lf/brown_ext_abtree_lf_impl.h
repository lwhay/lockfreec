/**
 * Implementation of the dictionary ADT with a lock-free relaxed (a,b)-tree.
 * Copyright (C) 2016 Trevor Brown
 * Contact (me [at] tbrown [dot] pro) with questions or comments.
 *
 * Details of the algorithm appear in Trevor's thesis:
 *    Techniques for Constructing Efficient Lock-free Data Structures. 2017.
 * 
 * The paper leaves it up to the implementer to decide when and how to perform
 * rebalancing steps. In this implementation, we keep track of violations and
 * fix them using a recursive cleanup procedure, which is designed as follows.
 * After performing a rebalancing step that replaced a set R of nodes,
 * recursive invocations are made for every violation that appears at a newly
 * created node. Thus, any violations that were present at nodes in R are either
 * eliminated by the rebalancing step, or will be fixed by recursive calls.
 * This way, if an invocation I of this cleanup procedure is trying to fix a
 * violation at a node that has been replaced by another invocation I' of
 * cleanup, then I can hand off responsibility for fixing the violation to I'.
 * Designing the rebalancing procedure to allow responsibility to be handed
 * off in this manner is not difficult; it simply requires going through each
 * rebalancing step S and determining which nodes involved in S can have
 * violations after S (and then making a recursive call for each violation).
 * 
 * -----------------------------------------------------------------------------
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ABTREE_IMPL_H
#define ABTREE_IMPL_H

#include "brown_ext_abtree_lf.h"

template<int DEGREE, typename K, class Compare, class RecManager>
abtree_ns::Node<DEGREE, K> *abtree_ns::abtree<DEGREE, K, Compare, RecManager>::allocateNode(const int tid) {
    Node<DEGREE, K> *newnode = recordmgr->template allocate<Node<DEGREE, K> >(tid);
    if (newnode == NULL) {
        COUTATOMICTID("ERROR: could not allocate node" << std::endl);
        exit(-1);
    }
    prov->initNode(newnode);
#ifdef GSTATS_HANDLE_STATS
    GSTATS_APPEND(tid, node_allocated_addresses, ((long long) newnode)%(1<<12));
#endif
    return newnode;
}

template<int DEGREE, typename K, class Compare, class RecManager>
const std::pair<void *, bool> abtree_ns::abtree<DEGREE, K, Compare, RecManager>::find(const int tid, const K &key) {
    std::pair<void *, bool> result;
    auto guard = recordmgr->getGuard(tid, true);
    Node<DEGREE, K> *l = entry->ptrs[0];
    while (!l->isLeaf()) {
        int ix = l->getChildIndex(key, cmp);
        l = l->ptrs[ix];
    }
    int index = l->getKeyIndex(key, cmp);
    if (index < l->getKeyCount() && l->keys[index] == key) {
        result.first = l->ptrs[index];
        result.second = true;
    } else {
        result.first = NO_VALUE;
        result.second = false;
    }
    return result;
}

template<int DEGREE, typename K, class Compare, class RecManager>
bool abtree_ns::abtree<DEGREE, K, Compare, RecManager>::contains(const int tid, const K &key) {
    return find(tid, key).second;
}

template<int DEGREE, typename K, class Compare, class RecManager>
int abtree_ns::abtree<DEGREE, K, Compare, RecManager>::rangeQuery(const int tid, const K &lo, const K &hi,
                                                                  K *const resultKeys, void **const resultValues) {
    setbench_error("not implemented");
}


template<int DEGREE, typename K, class Compare, class RecManager>
void *abtree_ns::abtree<DEGREE, K, Compare, RecManager>::doInsert(const int tid, const K &key, void *const value,
                                                                  const bool replace) {
    while (true) {
        /**
         * search
         */
        auto guard = recordmgr->getGuard(tid);
        Node<DEGREE, K> *gp = NULL;
        Node<DEGREE, K> *p = entry;
        Node<DEGREE, K> *l = p->ptrs[0];
        int ixToP = -1;
        int ixToL = 0;
        while (!l->isLeaf()) {
            ixToP = ixToL;
            ixToL = l->getChildIndex(key, cmp);
            gp = p;
            p = l;
            l = l->ptrs[ixToL];
        }

        /**
         * do the update
         */
        int keyIndex = l->getKeyIndex(key, cmp);
        if (keyIndex < l->getKeyCount() && l->keys[keyIndex] == key) {
            /**
             * if l already contains key, replace the existing value
             */
            void *const oldValue = l->ptrs[keyIndex];
            if (!replace) {
                return oldValue;
            }

            prov->scxInit(tid);

            // perform LLXs
            auto llxResult = prov->llx(tid, p);
            if (!prov->isSuccessfulLLXResult(llxResult) || p->ptrs[ixToL] != l) {
                continue; // retry the search
            }
            prov->scxAddNode(tid, p, false, llxResult);
            // no need to add l, since it is a leaf, and leaves are IMMUTABLE (so no point freezing or finalizing them)

            // create new node(s)
            Node<DEGREE, K> *n = allocateNode(tid);
            arraycopy(l->keys, 0, n->keys, 0, l->getKeyCount());
            arraycopy(l->ptrs, 0, n->ptrs, 0, l->getABDegree());
            n->ptrs[keyIndex] = (Node<DEGREE, K> *) value;
            n->leaf = true;
            n->searchKey = l->searchKey;
            n->size = l->size;
            n->weight = true;

            if (prov->scxExecute(tid, (void *volatile *) &p->ptrs[ixToL], l, n)) {
                this->recordmgr->retire(tid, l);
                fixDegreeViolation(tid, n);
                return oldValue;
            }
            this->recordmgr->deallocate(tid, n);

        } else {
            /**
             * if l does not contain key, we have to insert it
             */

            prov->scxInit(tid);

            auto llxResult = prov->llx(tid, p);
            if (!prov->isSuccessfulLLXResult(llxResult) || p->ptrs[ixToL] != l) {
                continue; // retry the search
            }
            prov->scxAddNode(tid, p, false, llxResult);
            // no need to add l, since it is a leaf, and leaves are IMMUTABLE (so no point freezing or finalizing them)

            if (l->getKeyCount() < b) {
                /**
                 * Insert std::pair
                 */

                // create new node(s)
                Node<DEGREE, K> *n = allocateNode(tid);
                arraycopy(l->keys, 0, n->keys, 0, keyIndex);
                arraycopy(l->keys, keyIndex, n->keys, keyIndex + 1, l->getKeyCount() - keyIndex);
                n->keys[keyIndex] = key;
                arraycopy(l->ptrs, 0, n->ptrs, 0, keyIndex);
                arraycopy(l->ptrs, keyIndex, n->ptrs, keyIndex + 1, l->getABDegree() - keyIndex);
                n->ptrs[keyIndex] = (Node<DEGREE, K> *) value;
                n->leaf = l->leaf;
                n->searchKey = l->searchKey;
                n->size = l->size + 1;
                n->weight = l->weight;

                if (prov->scxExecute(tid, (void *volatile *) &p->ptrs[ixToL], l, n)) {
                    recordmgr->retire(tid, l);
                    fixDegreeViolation(tid, n);
                    return NO_VALUE;
                }
                this->recordmgr->deallocate(tid, n);

            } else { // assert: l->getKeyCount() == DEGREE == b)
                /**
                 * Overflow
                 */

                // first, we create a std::pair of large arrays
                // containing too many keys and pointers to fit in a single node
                K keys[DEGREE + 1];
                Node<DEGREE, K> *ptrs[DEGREE + 1];
                arraycopy(l->keys, 0, keys, 0, keyIndex);
                arraycopy(l->keys, keyIndex, keys, keyIndex + 1, l->getKeyCount() - keyIndex);
                keys[keyIndex] = key;
                arraycopy(l->ptrs, 0, ptrs, 0, keyIndex);
                arraycopy(l->ptrs, keyIndex, ptrs, keyIndex + 1, l->getABDegree() - keyIndex);
                ptrs[keyIndex] = (Node<DEGREE, K> *) value;

                // create new node(s):
                // since the new arrays are too big to fit in a single node,
                // we replace l by a new subtree containing three new nodes:
                // a parent, and two leaves;
                // the array contents are then split between the two new leaves

                const int size1 = (DEGREE + 1) / 2;
                Node<DEGREE, K> *left = allocateNode(tid);
                arraycopy(keys, 0, left->keys, 0, size1);
                arraycopy(ptrs, 0, left->ptrs, 0, size1);
                left->leaf = true;
                left->searchKey = keys[0];
                left->size = size1;
                left->weight = true;

                const int size2 = (DEGREE + 1) - size1;
                Node<DEGREE, K> *right = allocateNode(tid);
                arraycopy(keys, size1, right->keys, 0, size2);
                arraycopy(ptrs, size1, right->ptrs, 0, size2);
                right->leaf = true;
                right->searchKey = keys[size1];
                right->size = size2;
                right->weight = true;

                Node<DEGREE, K> *n = allocateNode(tid);
                n->keys[0] = keys[size1];
                n->ptrs[0] = left;
                n->ptrs[1] = right;
                n->leaf = false;
                n->searchKey = keys[size1];
                n->size = 2;
                n->weight = p == entry;

                // note: weight of new internal node n will be zero,
                //       unless it is the root; this is because we test
                //       p == entry, above; in doing this, we are actually
                //       performing Root-Zero at the same time as this Overflow
                //       if n will become the root

                if (prov->scxExecute(tid, (void *volatile *) &p->ptrs[ixToL], l, n)) {
                    recordmgr->retire(tid, l);
                    // after overflow, there may be a weight violation at n
                    fixWeightViolation(tid, n);
                    return NO_VALUE;
                }
                this->recordmgr->deallocate(tid, n);
                this->recordmgr->deallocate(tid, left);
                this->recordmgr->deallocate(tid, right);
            }
        }
    }
}

template<int DEGREE, typename K, class Compare, class RecManager>
const std::pair<void *, bool> abtree_ns::abtree<DEGREE, K, Compare, RecManager>::erase(const int tid, const K &key) {
    while (true) {
        /**
         * search
         */
        auto guard = recordmgr->getGuard(tid);
        Node<DEGREE, K> *gp = NULL;
        Node<DEGREE, K> *p = entry;
        Node<DEGREE, K> *l = p->ptrs[0];
        int ixToP = -1;
        int ixToL = 0;
        while (!l->isLeaf()) {
            ixToP = ixToL;
            ixToL = l->getChildIndex(key, cmp);
            gp = p;
            p = l;
            l = l->ptrs[ixToL];
        }

        /**
         * do the update
         */
        const int keyIndex = l->getKeyIndex(key, cmp);
        if (keyIndex == l->getKeyCount() || l->keys[keyIndex] != key) {
            /**
             * if l does not contain key, we are done.
             */
            return std::pair<void *, bool>(NO_VALUE, false);
        } else {
            /**
             * if l contains key, replace l by a new copy that does not contain key.
             */

            prov->scxInit(tid);

            auto llxResult = prov->llx(tid, p);
            if (!prov->isSuccessfulLLXResult(llxResult) || p->ptrs[ixToL] != l) {
                continue; // retry the search
            }
            prov->scxAddNode(tid, p, false, llxResult);
            // no need to add l, since it is a leaf, and leaves are IMMUTABLE (so no point freezing or finalizing them)

            // create new node(s)
            Node<DEGREE, K> *n = allocateNode(tid);
            arraycopy(l->keys, 0, n->keys, 0, keyIndex);
            arraycopy(l->keys, keyIndex + 1, n->keys, keyIndex, l->getKeyCount() - (keyIndex + 1));
            arraycopy(l->ptrs, 0, n->ptrs, 0, keyIndex);
            arraycopy(l->ptrs, keyIndex + 1, n->ptrs, keyIndex, l->getABDegree() - (keyIndex + 1));
            n->leaf = true;
            n->searchKey = l->keys[0]; // NOTE: WE MIGHT BE DELETING l->keys[0], IN WHICH CASE newL IS EMPTY. HOWEVER, newL CAN STILL BE LOCATED BY SEARCHING FOR l->keys[0], SO WE USE THAT AS THE searchKey FOR newL.
            n->size = l->size - 1;
            n->weight = true;

            void *oldValue = l->ptrs[keyIndex];
            if (prov->scxExecute(tid, (void *volatile *) &p->ptrs[ixToL], l, n)) {
                recordmgr->retire(tid, l);
                /**
                 * Compress may be needed at p after removing key from l.
                 */
                fixDegreeViolation(tid, n);
                return std::pair<void *, bool>(oldValue, true);
            }
            this->recordmgr->deallocate(tid, n);
        }
    }
}

/**
 * 
 * 
 * IMPLEMENTATION OF REBALANCING
 * 
 * 
 */

template<int DEGREE, typename K, class Compare, class RecManager>
bool abtree_ns::abtree<DEGREE, K, Compare, RecManager>::fixWeightViolation(const int tid, Node<DEGREE, K> *viol) {
    if (viol->weight) return false;

    // assert: viol is internal (because leaves always have weight = 1)
    // assert: viol is not entry or root (because both always have weight = 1)

    // do an optimistic check to see if viol was already removed from the tree
    if (prov->llx(tid, viol) == prov->FINALIZED) {
        // recall that nodes are finalized precisely when
        // they are removed from the tree
        // we hand off responsibility for any violations at viol to the
        // process that removed it.
        return false;
    }

    // try to locate viol, and fix any weight violation at viol
    while (true) {
        const K k = viol->searchKey;
        Node<DEGREE, K> *gp = NULL;
        Node<DEGREE, K> *p = entry;
        Node<DEGREE, K> *l = p->ptrs[0];
        int ixToP = -1;
        int ixToL = 0;
        while (!l->isLeaf() && l != viol) {
            ixToP = ixToL;
            ixToL = l->getChildIndex(k, cmp);
            gp = p;
            p = l;
            l = l->ptrs[ixToL];
        }

        if (l != viol) {
            // l was replaced by another update.
            // we hand over responsibility for viol to that update.
            return false;
        }

        // we cannot apply this update if p has a weight violation
        // so, we check if this is the case, and, if so, try to fix it
        if (!p->weight) {
            fixWeightViolation(tid, p);
            continue;
        }

        prov->scxInit(tid);
        scx_handle_t llxResult;

        // perform LLXs 

        llxResult = prov->llx(tid, gp);
        if (!prov->isSuccessfulLLXResult(llxResult) || gp->ptrs[ixToP] != p) continue;
        prov->scxAddNode(tid, gp, false, llxResult);

        llxResult = prov->llx(tid, p);
        if (!prov->isSuccessfulLLXResult(llxResult) || p->ptrs[ixToL] != l) continue;
        prov->scxAddNode(tid, p, true, llxResult);

        if (!l->isLeaf()) {
            llxResult = prov->llx(tid, l);
            if (!prov->isSuccessfulLLXResult(llxResult)) continue;
            prov->scxAddNode(tid, l, true, llxResult);
        }

        const int c = p->getABDegree() + l->getABDegree();
        const int size = c - 1;

        if (size <= b) {
            /**
             * Absorb
             */

            // create new node(s)
            // the new arrays are small enough to fit in a single node,
            // so we replace p by a new internal node.
            Node<DEGREE, K> *n = allocateNode(tid);
            arraycopy_ptrs(p->ptrs, 0, n->ptrs, 0, ixToL);
            arraycopy_ptrs(l->ptrs, 0, n->ptrs, ixToL, l->getABDegree());
            arraycopy_ptrs(p->ptrs, ixToL + 1, n->ptrs, ixToL + l->getABDegree(), p->getABDegree() - (ixToL + 1));
            arraycopy(p->keys, 0, n->keys, 0, ixToL);
            arraycopy(l->keys, 0, n->keys, ixToL, l->getKeyCount());
            arraycopy(p->keys, ixToL, n->keys, ixToL + l->getKeyCount(), p->getKeyCount() - ixToL);
            n->leaf = false;
            assert(!l->isLeaf());
            n->searchKey = n->keys[0];
            n->size = size;
            n->weight = true;

            if (prov->scxExecute(tid, (void *volatile *) &gp->ptrs[ixToP], p, n)) {
                recordmgr->retire(tid, p);
                recordmgr->retire(tid, l);
                /**
                 * Compress may be needed at the new internal node we created
                 * (since we move grandchildren from two parents together).
                 */
                fixDegreeViolation(tid, n);
                return true;
            }
            this->recordmgr->deallocate(tid, n);

        } else {
            /**
             * Split
             */

            // merge keys of p and l into one big array (and similarly for children)
            // (we essentially replace the pointer to l with the contents of l)
            K keys[2 * DEGREE];
            Node<DEGREE, K> *ptrs[2 * DEGREE];
            arraycopy_ptrs(p->ptrs, 0, ptrs, 0, ixToL);
            arraycopy_ptrs(l->ptrs, 0, ptrs, ixToL, l->getABDegree());
            arraycopy_ptrs(p->ptrs, ixToL + 1, ptrs, ixToL + l->getABDegree(), p->getABDegree() - (ixToL + 1));
            arraycopy(p->keys, 0, keys, 0, ixToL);
            arraycopy(l->keys, 0, keys, ixToL, l->getKeyCount());
            arraycopy(p->keys, ixToL, keys, ixToL + l->getKeyCount(), p->getKeyCount() - ixToL);

            // the new arrays are too big to fit in a single node,
            // so we replace p by a new internal node and two new children.
            //
            // we take the big merged array and split it into two arrays,
            // which are used to create two new children u and v.
            // we then create a new internal node (whose weight will be zero
            // if it is not the root), with u and v as its children.

            // create new node(s)
            const int size1 = size / 2;
            Node<DEGREE, K> *left = allocateNode(tid);
            arraycopy(keys, 0, left->keys, 0, size1 - 1);
            arraycopy_ptrs(ptrs, 0, left->ptrs, 0, size1);
            left->leaf = false;
            assert(!l->isLeaf());
            left->searchKey = keys[0];
            left->size = size1;
            left->weight = true;

            const int size2 = size - size1;
            Node<DEGREE, K> *right = allocateNode(tid);
            arraycopy(keys, size1, right->keys, 0, size2 - 1);
            arraycopy_ptrs(ptrs, size1, right->ptrs, 0, size2);
            right->leaf = false;
            right->searchKey = keys[size1];
            right->size = size2;
            right->weight = true;

            Node<DEGREE, K> *n = allocateNode(tid);
            n->keys[0] = keys[size1 - 1];
            n->ptrs[0] = left;
            n->ptrs[1] = right;
            n->leaf = false;
            n->searchKey = keys[size1 - 1]; // note: should be the same as n->keys[0]
            n->size = 2;
            n->weight = (gp == entry);

            // note: weight of new internal node n will be zero,
            //       unless it is the root; this is because we test
            //       gp == entry, above; in doing this, we are actually
            //       performing Root-Zero at the same time as this Overflow
            //       if n will become the root

            if (prov->scxExecute(tid, (void *volatile *) &gp->ptrs[ixToP], p, n)) {
                recordmgr->retire(tid, p);
                recordmgr->retire(tid, l);

                fixWeightViolation(tid, n);
                fixDegreeViolation(tid, n);
                return true;
            }
            this->recordmgr->deallocate(tid, n);
            this->recordmgr->deallocate(tid, left);
            this->recordmgr->deallocate(tid, right);
        }
    }
}

template<int DEGREE, typename K, class Compare, class RecManager>
bool abtree_ns::abtree<DEGREE, K, Compare, RecManager>::fixDegreeViolation(const int tid, Node<DEGREE, K> *viol) {
    if (viol->getABDegree() >= a || viol == entry || viol == entry->ptrs[0]) {
        return false; // no degree violation at viol
    }

    // do an optimistic check to see if viol was already removed from the tree
    if (prov->llx(tid, viol) == prov->FINALIZED) {
        // recall that nodes are finalized precisely when
        // they are removed from the tree.
        // we hand off responsibility for any violations at viol to the
        // process that removed it.
        return false;
    }

    // we search for viol and try to fix any violation we find there
    // this entails performing AbsorbSibling or Distribute.
    while (true) {

        /**
         * search for viol
         */
        const K k = viol->searchKey;
        Node<DEGREE, K> *gp = NULL;
        Node<DEGREE, K> *p = entry;
        Node<DEGREE, K> *l = p->ptrs[0];
        int ixToP = -1;
        int ixToL = 0;
        while (!l->isLeaf() && l != viol) {
            ixToP = ixToL;
            ixToL = l->getChildIndex(k, cmp);
            gp = p;
            p = l;
            l = l->ptrs[ixToL];
        }

        if (l != viol) {
            // l was replaced by another update.
            // we hand over responsibility for viol to that update.
            return false;
        }

        // assert: gp != NULL (because if AbsorbSibling or Distribute can be applied, then p is not the root)

        prov->scxInit(tid);
        scx_handle_t llxResult;

        // perform LLXs 

        llxResult = prov->llx(tid, gp);
        if (!prov->isSuccessfulLLXResult(llxResult) || gp->ptrs[ixToP] != p) continue;
        prov->scxAddNode(tid, gp, false, llxResult);

        llxResult = prov->llx(tid, p);
        if (!prov->isSuccessfulLLXResult(llxResult) || p->ptrs[ixToL] != l) continue;
        prov->scxAddNode(tid, p, true, llxResult);

        int ixToS = (ixToL > 0 ? ixToL - 1 : 1);
        Node<DEGREE, K> *s = p->ptrs[ixToS];

        // we can only apply AbsorbSibling or Distribute if there are no
        // weight violations at p, l or s.
        // so, we first check for any weight violations,
        // and fix any that we see.
        bool foundWeightViolation = false;
        if (!p->weight) {
            foundWeightViolation = true;
            fixWeightViolation(tid, p);
        }
        if (!l->weight) {
            foundWeightViolation = true;
            fixWeightViolation(tid, l);
        }
        if (!s->weight) {
            foundWeightViolation = true;
            fixWeightViolation(tid, s);
        }
        // if we see any weight violations, then either we fixed one,
        // removing one of these nodes from the tree,
        // or one of the nodes has been removed from the tree by another
        // rebalancing step, so we retry the search for viol
        if (foundWeightViolation) continue;

        // assert: there are no weight violations at p, l or s
        // assert: l and s are either both leaves or both internal nodes
        //         (because there are no weight violations at these nodes)

        // also note that p->size >= a >= 2

        Node<DEGREE, K> *left;
        Node<DEGREE, K> *right;
        int leftindex;
        int rightindex;

        if (ixToL < ixToS) {
            left = l;
            right = s;
            leftindex = ixToL;
            rightindex = ixToS;
        } else {
            left = s;
            right = l;
            leftindex = ixToS;
            rightindex = ixToL;
        }

        // since both left and right have weight 0, if one is a leaf, then both are.
        // so, we can test one, and perform llx on both or neither, as appropriate.
        if (!left->isLeaf()) {
            llxResult = prov->llx(tid, left);
            if (!prov->isSuccessfulLLXResult(llxResult)) continue;
            prov->scxAddNode(tid, left, true, llxResult);

            llxResult = prov->llx(tid, right);
            if (!prov->isSuccessfulLLXResult(llxResult)) continue;
            prov->scxAddNode(tid, right, true, llxResult);
        }

        int sz = left->getABDegree() + right->getABDegree();
        assert(left->weight && right->weight);

        if (sz < 2 * a) {
            /**
             * AbsorbSibling
             */

            // create new node(s))
            Node<DEGREE, K> *newl = allocateNode(tid);
            int k1 = 0, k2 = 0;
            for (int i = 0; i < left->getKeyCount(); ++i) {
                newl->keys[k1++] = left->keys[i];
            }
            for (int i = 0; i < left->getABDegree(); ++i) {
                if (left->isLeaf()) {
                    newl->ptrs[k2++] = left->ptrs[i];
                } else {
                    //assert(left->getKeyCount() != left->getABDegree());
                    newl->ptrs[k2++] = left->ptrs[i];
                }
            }
            if (!left->isLeaf()) newl->keys[k1++] = p->keys[leftindex];
            for (int i = 0; i < right->getKeyCount(); ++i) {
                newl->keys[k1++] = right->keys[i];
            }
            for (int i = 0; i < right->getABDegree(); ++i) {
                if (right->isLeaf()) {
                    newl->ptrs[k2++] = right->ptrs[i];
                } else {
                    newl->ptrs[k2++] = right->ptrs[i];
                }
            }
            newl->leaf = left->isLeaf();
            newl->searchKey = l->searchKey;
            newl->size = l->getABDegree() + s->getABDegree();
            newl->weight = true;
            assert(left->weight && right->weight && p->weight);

            // now, we atomically replace p and its children with the new nodes.
            // if appropriate, we perform RootAbsorb at the same time.
            if (gp == entry && p->getABDegree() == 2) {
                if (prov->scxExecute(tid, (void *volatile *) &gp->ptrs[ixToP], p, newl)) {
                    recordmgr->retire(tid, p);
                    recordmgr->retire(tid, l);
                    recordmgr->retire(tid, s);

                    fixDegreeViolation(tid, newl);
                    return true;
                }
                this->recordmgr->deallocate(tid, newl);

            } else {
                assert(gp != entry || p->getABDegree() > 2);

                // create n from p by:
                // 1. skipping the key for leftindex and child pointer for ixToS
                // 2. replacing l with newl
                Node<DEGREE, K> *n = allocateNode(tid);
                for (int i = 0; i < leftindex; ++i) {
                    n->keys[i] = p->keys[i];
                }
                for (int i = 0; i < ixToS; ++i) {
                    n->ptrs[i] = p->ptrs[i];
                }
                for (int i = leftindex + 1; i < p->getKeyCount(); ++i) {
                    n->keys[i - 1] = p->keys[i];
                }
                for (int i = ixToL + 1; i < p->getABDegree(); ++i) {
                    n->ptrs[i - 1] = p->ptrs[i];
                }
                // replace l with newl in n's pointers
                n->ptrs[ixToL - (ixToL > ixToS)] = newl;
                n->leaf = false;
                n->searchKey = p->searchKey;
                n->size = p->getABDegree() - 1;
                n->weight = true;

                if (prov->scxExecute(tid, (void *volatile *) &gp->ptrs[ixToP], p, n)) {
                    recordmgr->retire(tid, p);
                    recordmgr->retire(tid, l);
                    recordmgr->retire(tid, s);

                    fixDegreeViolation(tid, newl);
                    fixDegreeViolation(tid, n);
                    return true;
                }
                this->recordmgr->deallocate(tid, newl);
                this->recordmgr->deallocate(tid, n);
            }

        } else {
            /**
             * Distribute
             */

            int leftsz = sz / 2;
            int rightsz = sz - leftsz;

            // create new node(s))
            Node<DEGREE, K> *n = allocateNode(tid);
            Node<DEGREE, K> *newleft = allocateNode(tid);
            Node<DEGREE, K> *newright = allocateNode(tid);

            // combine the contents of l and s (and one key from p if l and s are internal)
            K keys[2 * DEGREE];
            Node<DEGREE, K> *ptrs[2 * DEGREE];
            int k1 = 0, k2 = 0;
            for (int i = 0; i < left->getKeyCount(); ++i) {
                keys[k1++] = left->keys[i];
            }
            for (int i = 0; i < left->getABDegree(); ++i) {
                if (left->isLeaf()) {
                    ptrs[k2++] = left->ptrs[i];
                } else {
                    ptrs[k2++] = left->ptrs[i];
                }
            }
            if (!left->isLeaf()) keys[k1++] = p->keys[leftindex];
            for (int i = 0; i < right->getKeyCount(); ++i) {
                keys[k1++] = right->keys[i];
            }
            for (int i = 0; i < right->getABDegree(); ++i) {
                if (right->isLeaf()) {
                    ptrs[k2++] = right->ptrs[i];
                } else {
                    ptrs[k2++] = right->ptrs[i];
                }
            }

            // distribute contents between newleft and newright
            k1 = 0;
            k2 = 0;
            for (int i = 0; i < leftsz - !left->isLeaf(); ++i) {
                newleft->keys[i] = keys[k1++];
            }
            for (int i = 0; i < leftsz; ++i) {
                if (left->isLeaf()) {
                    newleft->ptrs[i] = ptrs[k2++];
                } else {
                    newleft->ptrs[i] = ptrs[k2++];
                }
            }
            newleft->leaf = left->isLeaf();
            newleft->searchKey = newleft->keys[0];
            newleft->size = leftsz;
            newleft->weight = true;

            // reserve one key for the parent (to go between newleft and newright)
            K keyp = keys[k1];
            if (!left->isLeaf()) ++k1;
            for (int i = 0; i < rightsz - !left->isLeaf(); ++i) {
                newright->keys[i] = keys[k1++];
            }
            for (int i = 0; i < rightsz; ++i) {
                if (right->isLeaf()) {
                    newright->ptrs[i] = ptrs[k2++];
                } else {
                    newright->ptrs[i] = ptrs[k2++];
                }
            }
            newright->leaf = right->isLeaf();
            newright->searchKey = newright->keys[0];
            newright->size = rightsz;
            newright->weight = true;

            // create n from p by replacing left with newleft and right with newright,
            // and replacing one key (between these two pointers)
            for (int i = 0; i < p->getKeyCount(); ++i) {
                n->keys[i] = p->keys[i];
            }
            for (int i = 0; i < p->getABDegree(); ++i) {
                n->ptrs[i] = p->ptrs[i];
            }
            n->keys[leftindex] = keyp;
            n->ptrs[leftindex] = newleft;
            n->ptrs[rightindex] = newright;
            n->leaf = false;
            n->searchKey = p->searchKey;
            n->size = p->size;
            n->weight = true;

            if (prov->scxExecute(tid, (void *volatile *) &gp->ptrs[ixToP], p, n)) {
                recordmgr->retire(tid, p);
                recordmgr->retire(tid, l);
                recordmgr->retire(tid, s);

                fixDegreeViolation(tid, n);
                return true;
            }
            this->recordmgr->deallocate(tid, n);
            this->recordmgr->deallocate(tid, newleft);
            this->recordmgr->deallocate(tid, newright);
        }
    }
}

#endif    /* ABTREE_IMPL_H */
