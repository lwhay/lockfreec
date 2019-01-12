#include "lf_memtrie.h"

atom_t trie_new(struct trie_state **trie, atom_t trie_node_num) {
    *trie = (struct trie_state *) aligned_malloc(sizeof(struct trie_state), LF_ALIGN_DOUBLE_POINTER);
    if (*trie) {
        (*trie)->init_node_counter = trie_node_num;
        trie_new_trie_entry(&(*trie)->trie_entry);
        trie_new_nodelist(&(*trie)->trieNodeList, trie_node_num);
    }
    if (!((*trie)->trieNodeList && (*trie)->trie_entry)) {
        if ((*trie)->trie_entry)
            aligned_free((*trie)->trie_entry);
        if ((*trie)->trieNodeList)
            trie_nodelist_free((*trie)->trieNodeList);
        if (*trie)
            aligned_free(*trie);
        return 0;
    }

    LF_BARRIER_STORE;
    return 1;
}

atom_t trie_new_trie_entry(struct trie **trie) {
    assert(trie != NULL);
    atom_t rv = 0;
    *trie = (struct trie *) aligned_malloc(sizeof(struct trie), LF_ALIGN_DOUBLE_POINTER);
    if (*trie) {
        memset(*trie, 0, sizeof(struct trie));
        (*trie)->root = (struct trie_node *) aligned_malloc(sizeof(struct trie_node), LF_ALIGN_DOUBLE_POINTER);
        memset((*trie)->root, 0, sizeof(struct trie_node));
        rv = 1;
    }
    return rv;
}

atom_t trie_new_nodelist(struct trie_node_list **nodelist, atom_t trie_node_num) {
    int rv = 0;
    atom_t element_count;
    *nodelist = (struct trie_node_list *) aligned_malloc(sizeof(struct trie_node_list), LF_ALIGN_DOUBLE_POINTER);

    if ((*nodelist) != NULL) {
        element_count = trie_nodelist_new_elements(*nodelist, trie_node_num);
        if (element_count == trie_node_num)
            rv = 1;
        if (element_count != trie_node_num) {
            trie_nodelist_free(*nodelist);
            aligned_free(*nodelist);
            *nodelist = NULL;
        }
    }
    LF_BARRIER_STORE;
    return (rv);
}

atom_t trie_nodelist_new_elements(struct trie_node_list *nodelist, atom_t trie_node_num) {
    struct trie_node *tnode;
    atom_t loop, count = 0;
    assert(nodelist != NULL);
    for (loop = 0; loop < trie_node_num; loop++) {
        tnode = (struct trie_node *) aligned_malloc(sizeof(struct trie_node), LF_ALIGN_DOUBLE_POINTER);
        if (tnode != NULL) {
            memset(tnode, 0, sizeof(struct trie_node));
            tnode_push(nodelist, tnode);
            count++;
        }
    }
    LF_BARRIER_STORE;
    return (count);
}

void tnode_push(struct trie_node_list *nodelist, struct trie_node *tnode) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct trie_node *tnode_local, *tnode_original;
    tnode_local = tnode;
    LF_BARRIER_LOAD;
    tnode_original = nodelist->first;
    do {
        tnode_local->_brotherNode = tnode_original;
    } while (tnode_local->_brotherNode !=
             (tnode_original = (struct trie_node *) abstraction_cas((volatile atom_t *) &nodelist->first,
                                                                    (atom_t) tnode_local, (atom_t) tnode_original)));
    abstraction_increment((volatile atom_t *) &nodelist->nodelist_counter);
    LF_BARRIER_STORE;
    return;
}

atom_t tnode_pop(struct trie_node_list *nodelist, struct trie_node **tnode) {
    atom_t rv = 0;
    assert(nodelist != NULL);
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct trie_node *tnode_local, *tnode_original;
    tnode_local = *tnode;
    LF_BARRIER_LOAD;
    tnode_local = nodelist->first;
    do {
        tnode_original = tnode_local;
        if (nodelist->first == NULL /*|| tnode_local->_brotherNode == NULL*/) {
            *tnode = NULL;
            return rv;
        }
    } while (tnode_original != (tnode_local = (struct trie_node *) abstraction_cas((volatile atom_t *) &nodelist->first,
                                                                                   (atom_t) tnode_local->_brotherNode,
                                                                                   (atom_t) tnode_original)));
    abstraction_sub((volatile atom_t *) &nodelist->nodelist_counter);
    tnode_local->_brotherNode = NULL;
    *tnode = tnode_local;
    rv = 1;
    return rv;
}

/***********************insert trie node ******************/

atom_t lf_trie_insert_real_data(struct trie_state *trie_state, unsigned char *in_str, unsigned strlen) {
    unsigned pos = 0;
    int found = 1;
    //printf("\ntest_str:%s\n",in_str);
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct trie_node *node, *prev_next = 0x0, *prev = 0x0, *parrent = 0x0;
    node = trie_state->trie_entry->root;
    prev = node;
    // Look for the part of the word which is in trie
    while (found && pos < strlen) {
        found = 0;
        parrent = node;
        node = get_subnode_by_label(node, &prev, &prev_next, in_str[pos]);
        if (node) {
            found = 1;
            ++pos;
        }
    }

    // Add part of the word which is not in trie
    if (!node || pos != strlen) {
        node = parrent;
        unsigned i;
        for (i = pos; i < strlen; ++i) {
            node = lf_trie_insert_node(trie_state, node, &prev, &prev_next, in_str[pos]);
            ++pos;
            if (node == NULL)
                return TRIE_INSERT_ERROR;
        }
        return TRIE_INSERT_SUCCESS;
    }
    assert(node != 0x0);
    return TRIE_INSERT_DUPLICATION;
}

atom_t lf_trie_search_real_data(struct trie_state *trie_state, unsigned char *in_str, unsigned strlen) {
    unsigned pos = 0;
    int found = 1;
    //printf("\ntest_str:%s\n",in_str);
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct trie_node *node, *prev_next = 0x0, *prev = 0x0, *parrent = 0x0;
    node = trie_state->trie_entry->root;
    prev = node;
    // Look for the part of the word which is in trie
    while (found && pos < strlen) {
        found = 0;
        parrent = node;
        node = search_subnode_by_label(node, in_str[pos]);
        if (node) {
            found = 1;
            ++pos;
        }
    }

    // Add part of the word which is not in trie
    if (!node || pos != strlen) {
        return TRIE_SEARCH_ERROR;
    }

    return TRIE_SEARCH_SUCCESS;
}


struct trie_node *
lf_trie_insert_node(struct trie_state *trie_state, struct trie_node *parrent, struct trie_node **prev,
                    struct trie_node **prev_next,
                    unsigned char key) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct trie_node *tnode, *rv_node, *prev_cmp;
    int cas_result = 0;
    if (!tnode_pop(trie_state->trieNodeList, &tnode)) {
        return NULL;
//        if(trie_nodelist_new_elements(trie_state->trieNodeList,TRIE_NEW_NODELIST_NUM))
//            return lf_trie_insert_node(trie_state,parrent,prev,prev_next,key);
//        else return NULL;
    } else {
        tnode->key = key;

        do {
            //locate the insert point
            if ((rv_node = get_subnode_by_label(parrent, prev, prev_next, key)) != NULL) {
                tnode->_brotherNode = NULL;
                tnode_push(trie_state->trieNodeList, tnode);
                return rv_node;
            }
            //start insert
            prev_cmp = *prev_next;
            tnode->_brotherNode = prev_cmp;
            if (*prev == parrent) {
                *prev_next = (struct trie_node *) abstraction_cas((volatile atom_t *) &(*prev)->_firstSubNode,
                                                                  (atom_t) tnode, (atom_t) prev_cmp);
            } else {
                *prev_next = (struct trie_node *) abstraction_cas((volatile atom_t *) &(*prev)->_brotherNode,
                                                                  (atom_t) tnode, (atom_t) prev_cmp);
            }
            if (prev_cmp == *prev_next) {
                rv_node = tnode;
                *prev = tnode;
                *prev_next = tnode->_firstSubNode;
                abstraction_increment((volatile atom_t *) &(trie_state->trie_entry->counter));
                cas_result = 1;
            }
        } while (cas_result == 0);
        return rv_node;
    }
}

struct trie_node *search_subnode_by_label(struct trie_node *parrent, unsigned char key) {
    assert(parrent != NULL);
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct trie_node *tmp_node;
    atom_t cas_result = 0;
    tmp_node = parrent->_firstSubNode;
    while (cas_result == 0) {
        if (tmp_node == NULL || tmp_node->key > key)
            return NULL;
        else if (tmp_node->key < key) {
            tmp_node = tmp_node->_brotherNode;
        } else
            return tmp_node;
    }
    return NULL;
}

struct trie_node *
get_subnode_by_label(struct trie_node *parrent, struct trie_node **prev, struct trie_node **prev_next,
                     unsigned char key) {
    assert(parrent != NULL);
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct trie_node *tmp_node;
    int cas_result = 0;

    tmp_node = (parrent)->_firstSubNode;
    if (tmp_node == NULL || tmp_node->key > key) {
        *prev = parrent;
        *prev_next = tmp_node;
        return NULL;
    } else if (tmp_node->key < key) {
        if ((*prev == parrent)) {
            *prev = tmp_node;
            *prev_next = (*prev)->_brotherNode;
        }
    } else {
        *prev = tmp_node;
        *prev_next = (*prev)->_firstSubNode;
        return tmp_node;
    }

    while (cas_result == 0) {
        if ((*prev_next) && (*prev_next)->key < key) {
            *prev = *prev_next;
            *prev_next = (*prev_next)->_brotherNode;
        } else if (!(*prev_next) || (*prev_next)->key > key) {
            cas_result = 1;
            return NULL;
        } else {
            *prev = *prev_next;
            *prev_next = (*prev)->_firstSubNode;
            return *prev;
        }
    }
    return NULL;
}

#pragma warning(disable : 4100)

void trie_use(struct trie_state *ts) {
    assert (NULL != ts);
    LF_BARRIER_LOAD;
    return;
}

//#pragma warning(default : 4100)

#pragma warning(disabe : 4100)

/****************************insert trie node over **********/

/*------------------aligned free memory -----------*/
void trie_state_free(struct trie_state *trie_state) {
    if (trie_state != NULL) {
        if (trie_state->trieNodeList != NULL) {
            trie_nodelist_free(trie_state->trieNodeList);
        }

        if (trie_state->trie_entry != NULL) {
            trie_entry_free(trie_state->trie_entry);
        }
        aligned_free(trie_state);
    }
    return;
}

void trie_nodelist_free(struct trie_node_list *nodelist) {
    assert(nodelist != NULL);
    struct trie_node *tnode;
    while (tnode_pop(nodelist, &tnode)) {
        aligned_free(tnode);
    }
    aligned_free(nodelist);
    return;
}

void trie_entry_free(struct trie *trie) {
    assert(trie != NULL);
    struct trie_node *tnode;
    //todo
    aligned_free(trie);
    return;
}
