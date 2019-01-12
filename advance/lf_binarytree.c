#include "lf_binarytree.h"

atom_t binarytree_new(struct binarytree_state **state, atom_t node_num) {
    *state = (struct binarytree_state *) aligned_malloc(sizeof(struct binarytree_state), LF_ALIGN_DOUBLE_POINTER);
    if (*state) {
        (*state)->init_node_counter = node_num;
        binarytree_new_entry(&(*state)->tree);
        binarytree_new_nodelist((*state)->nodelist, node_num);
    }
    if (!((*state)->nodelist && (*state)->tree)) {
        if ((*state)->tree)
            aligned_free((*state)->tree);
        if ((*state)->nodelist)
            aligned_free((*state)->nodelist);
        if (*state)
            aligned_free(*state);
    }
    LF_BARRIER_STORE;
    return 1;
}

atom_t binarytree_new_entry(struct binarytree **tree) {
    assert(tree != NULL);
    atom_t rv = 0;
    *tree = (struct binarytree *) aligned_malloc(sizeof(struct binarytree), LF_ALIGN_DOUBLE_POINTER);
    if (*tree) {
        memset(*tree, 0, sizeof(struct binarytree));
        (*tree)->root = (struct binary_node *) aligned_malloc(sizeof(struct binary_node), LF_ALIGN_DOUBLE_POINTER);
        memset((*tree)->root, 0, sizeof(struct binary_node));
        rv = 1;
    }
    return rv;
}

atom_t binarytree_new_nodelist(struct binary_node_list **nodelist, atom_t node_num) {
    int rv = 0;
    atom_t element_count;
    *nodelist = (struct binary_node_list *) aligned_malloc(sizeof(struct binary_node_list), LF_ALIGN_DOUBLE_POINTER);
    if ((*nodelist) != NULL) {
        element_count = binarytree_nodelist_new_elements(*nodelist, node_num);
        if (element_count == node_num) {
            rv = 1;
        } else {
            lf_binarytree_nodelist_free(*nodelist);
            aligned_free(*nodelist);
            *nodelist = NULL;
        }
    }
    LF_BARRIER_STORE;
    return rv;
}

atom_t binarytree_nodelist_new_elements(struct binary_node_list *nodelist, atom_t node_num) {
    struct binary_node *node;
    atom_t loop, count = 0;
    assert(nodelist != NULL);
    for (loop = 0; loop < node_num; loop++) {
        node = (struct binary_node *) aligned_malloc(sizeof(struct binary_node), LF_ALIGN_DOUBLE_POINTER);
        if (node != NULL) {
            memset(node, 0, sizeof(struct binary_node));
            binarytree_node_push(nodelist, node);
            count++;
        }
    }
    LF_BARRIER_STORE;
    return count;
}

void binarytree_use(struct binarytree_state *state) {
    assert(NULL != state);
    LF_BARRIER_LOAD;
}

void binarytree_node_push(struct binary_node_list *nodelist, struct binary_node *node) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct binary_node *local_node, *original_node;
    local_node = node;
    LF_BARRIER_LOAD;
    original_node = nodelist->first;
    do {
        local_node->_right = original_node;
    } while (local_node->_right !=
             (original_node = (struct binary_node *) abstraction_cas((volatile atom_t *) &nodelist->first,
                                                                     (atom_t) local_node, (atom_t) original_node)));
    abstraction_increment((volatile atom_t *) &nodelist->nodelist_counter);
    LF_BARRIER_STORE;
}

atom_t binarytree_node_pop(struct binary_node_list *nodelist, struct binary_node **node) {
    atom_t rv = 0;
    assert(nodelist != NULL);
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct binary_node *local_node, *original_node;
    local_node = *node;
    LF_BARRIER_LOAD;
    local_node = nodelist->first;
    do {
        original_node = local_node;
        if (nodelist->first == NULL) {
            *node = NULL;
            return rv;
        }
    } while (original_node != (local_node = (struct binary_node *) abstraction_cas((volatile atom_t *) &nodelist->first,
                                                                                   (atom_t) local_node->_right,
                                                                                   (atom_t) original_node)));
    abstraction_sub((volatile atom_t *) &nodelist->nodelist_counter);
    local_node->_right = NULL;
    *node = local_node;
    rv = 1;
    return rv;
}

// Self-operations, will balancing the tree in the future.
struct binary_node *
binarytree_get_subnode(struct binarytree_state *state, struct binary_node *parent, struct binary_node **prev,
                       struct binary_node **prev_next, void *data) {
    assert(parent != NULL);
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct binary_node *node;
    int cas_result = 0;
    return NULL;
}

struct binary_node *binarytree_search_subnode(struct binarytree_state *state, struct binary_node *parent, void *data) {
    assert(parent != NULL);
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct binary_node *tmp_node;
    switch (state->tree->cmp_func(parent->data, data)) {
        case BINARY_HIT_LEFT:
            return parent->_left;
        case BINARY_HIT_EQUAL:
            return parent;
        case BINARY_HIT_RIGHT:
            return parent->_right;
        default:
            return NULL;
    }
}

// Operations
atom_t lf_binarytree_insert(struct binarytree_state *state, void *data) {
    return 0;
}

atom_t lf_binarytree_search(struct binarytree_state *state, void *data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct binary_node *node, *current;
    current = state->tree->root;
    while (1) {
        node = binarytree_search_subnode(state, current, data);
        if (node == current) {
            return BINARY_SEARCH_SUCCESS;
        } else if (node == NULL) {
            return BINARY_SEARCH_ERROR;
        }
    }
}

struct BinaryTreeNode *
lf_binarytree_insert_node(struct binarytree_state *state, struct binary_node *parent, struct binary_node **prev,
                          struct binary_node **prev_next, void *data) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct binary_node *node, *rv_node, *prev_tmp;
    int cas_result = 0;
    if (!binarytree_node_pop(state->nodelist, &node)) {
        return NULL;
    } else {
        node->data = data;
        do {
            if ((rv_node = binarytree_get_subnode(state, parent, prev, prev_next, data)) != NULL) {
                node->_right = NULL;
                binarytree_node_push(state->nodelist, node);
                return rv_node;
            }
        } while (cas_result == 0);
        return rv_node;
    }
}

void lf_binarytree_state_free(struct binarytree_state *state) {
    if (state != NULL) {
        if (state->nodelist != NULL) {
            lf_binarytree_nodelist_free(state->nodelist);
        }
        if (state->tree != NULL) {
            lf_binarytree_free(state->tree);
        }
        aligned_free(state);
    }
}

void lf_binarytree_nodelist_free(struct binary_node_list *nodelist) {
    assert(nodelist != NULL);
    struct binarytree_node *node;
    while (binarytree_node_pop(nodelist, &node)) {
        aligned_free(node);
    }
    aligned_free(nodelist);
}

void lf_binarytree_free(struct binarytree *tree) {
    assert(tree != NULL);
    struct binary_node *node;
    //todo
    aligned_free(tree);
}

int lf_binarytree_int_comparator(void *d1, void *d2) {
    return *(int *) d2 - *(int *) d1;
}