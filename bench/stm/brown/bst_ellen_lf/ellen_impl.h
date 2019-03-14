/* 
 * File:   ellen_impl.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 1, 2017, 4:02 PM
 */

#ifndef ELLEN_IMPL_H
#define ELLEN_IMPL_H

#include "ellen.h"

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ellen<skey_t, sval_t, RecMgr>::bst_find(const int tid, skey_t key) {
    auto guard = recmgr->getGuard(tid, true);

    auto l = root->left;
    while (l->left) l = (key < l->key) ? l->left : l->right;
    return (l->key == key) ? l->value : NO_VALUE;
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ellen<skey_t, sval_t, RecMgr>::bst_insert(const int tid, const skey_t key, const sval_t value) {
    while (1) {
        auto guard = recmgr->getGuard(tid);

        auto p = root;
        auto pupdate = p->update;
        SOFTWARE_BARRIER;
        auto l = p->left;
        while (l->left) {
            p = l;
            pupdate = p->update;
            SOFTWARE_BARRIER;
            l = (key < l->key) ? l->left : l->right;
        }
        if (l->key == key) {
            return l->value;
        }
        if (GETFLAG(pupdate) != STATE_CLEAN) {
            bst_help(tid, pupdate);
        } else {
            auto new_node = create_node(tid, key, value, NULL, NULL);
            auto new_sibling = create_node(tid, l->key, l->value, NULL, NULL);
            auto new_internal = (key < l->key)
                                ? create_node(tid, l->key, NO_VALUE, new_node, new_sibling)
                                : create_node(tid, key, NO_VALUE, new_sibling, new_node);
            auto op = create_iinfo_t(tid, p, new_internal, l);
            auto result = CASV(&p->update, pupdate, FLAG(op, STATE_IFLAG));
            if (result == pupdate) {
                bst_help_insert(tid, op);
                return NO_VALUE;
            } else {
                recmgr->deallocate(tid, new_node);
                recmgr->deallocate(tid, new_sibling);
                recmgr->deallocate(tid, new_internal);
                recmgr->deallocate(tid, op);
                bst_help(tid, result);
            }
        }
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
sval_t ellen<skey_t, sval_t, RecMgr>::bst_delete(const int tid, skey_t key) {
    while (1) {
        auto guard = recmgr->getGuard(tid);

        node_t<skey_t, sval_t> *gp = NULL;
        info_t<skey_t, sval_t> *gpupdate = NULL;
        auto p = root;
        auto pupdate = p->update;
        SOFTWARE_BARRIER;
        auto l = p->left;
        while (l->left) {
            gp = p;
            p = l;
            gpupdate = pupdate;
            pupdate = p->update;
            SOFTWARE_BARRIER;
            l = (key < l->key) ? l->left : l->right;
        }
        if (l->key != key) {
            return NO_VALUE;
        }
        auto found_value = l->value;
        if (GETFLAG(gpupdate) != STATE_CLEAN) {
            bst_help(tid, gpupdate);
        } else if (GETFLAG(pupdate) != STATE_CLEAN) {
            bst_help(tid, pupdate);
        } else {
            auto op = create_dinfo_t(tid, gp, p, l, pupdate);
            auto result = CASV(&gp->update, gpupdate, FLAG(op, STATE_DFLAG));
            if (result == gpupdate) {
                if (bst_help_delete(tid, op)) return found_value;
            } else {
                recmgr->deallocate(tid, op);
                bst_help(tid, result);
            }
        }
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
bool
ellen<skey_t, sval_t, RecMgr>::bst_cas_child(const int tid, node_t<skey_t, sval_t> *parent, node_t<skey_t, sval_t> *old,
                                             node_t<skey_t, sval_t> *nnode) {
    if (old == parent->left) {
        return CASB(&parent->left, old, nnode);
    } else if (old == parent->right) {
        return CASB(&parent->right, old, nnode);
    } else {
        return false;
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
void ellen<skey_t, sval_t, RecMgr>::bst_help_insert(const int tid, info_t<skey_t, sval_t> *op) {
    if (bst_cas_child(tid, op->iinfo.p, op->iinfo.l, op->iinfo.new_internal)) {
        recmgr->retire(tid, op->iinfo.l);
    }
    if (CASB(&op->iinfo.p->update, FLAG(op, STATE_IFLAG), FLAG(op, STATE_CLEAN))) {
        recmgr->retire(tid, op);
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
bool ellen<skey_t, sval_t, RecMgr>::bst_help_delete(const int tid, info_t<skey_t, sval_t> *op) {
    auto result = CASV(&op->dinfo.p->update, op->dinfo.pupdate, FLAG(op, STATE_MARK));
    if ((result == op->dinfo.pupdate) || (result == FLAG(op, STATE_MARK))) {
        bst_help_marked(tid, op);
        return true;
    } else {
        bst_help(tid, result);
        if (CASB(&op->dinfo.gp->update, FLAG(op, STATE_DFLAG), FLAG(op, STATE_CLEAN))) {
            recmgr->retire(tid, op);
        }
        return false;
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
void ellen<skey_t, sval_t, RecMgr>::bst_help_marked(const int tid, info_t<skey_t, sval_t> *op) {
    node_t<skey_t, sval_t> *other;
    if (op->dinfo.p->right == op->dinfo.l) {
        other = op->dinfo.p->left;
    } else {
        other = op->dinfo.p->right;
    }
    if (bst_cas_child(tid, op->dinfo.gp, op->dinfo.p, other)) {
        recmgr->retire(tid, op->dinfo.l);
        recmgr->retire(tid, op->dinfo.p);
    }
    if (CASB(&op->dinfo.gp->update, FLAG(op, STATE_DFLAG), FLAG(op, STATE_CLEAN))) {
        recmgr->retire(tid, op);
    }
}

template<typename skey_t, typename sval_t, class RecMgr>
void ellen<skey_t, sval_t, RecMgr>::bst_help(const int tid, info_t<skey_t, sval_t> *u) {
    if (GETFLAG(u) == STATE_DFLAG) {
        bst_help_delete(tid, UNFLAG(u));
    } else if (GETFLAG(u) == STATE_IFLAG) {
        bst_help_insert(tid, UNFLAG(u));
    } else if (GETFLAG(u) == STATE_MARK) {
        bst_help_marked(tid, UNFLAG(u));
    }
}

#endif /* ELLEN_IMPL_H */

