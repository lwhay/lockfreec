#ifndef LF_BINARYTREE_H
#define LF_BINARYTREE_H

#include <string.h>
#include "lf_basic.h"

#define BINARY_HIT_LEFT                   -1
#define BINARY_HIT_EQUAL                  0
#define BINARY_HIT_RIGHT                  1
#define BINARY_INSERT_SUCCESS             1
#define BINARY_INSERT_DUPLICATION         2
#define BINARY_INSERT_ERROR               3
#define BINARY_SEARCH_SUCCESS             1
#define BINARY_SEARCH_ERROR               2
#define BINARY_NEW_NODELIST_NUM           200000000

struct binary_node {
    struct binary_node *volatile _left;
    struct binary_node *volatile _right;
    void *data;  //payload;
};

struct binarytree {
    struct binary_node *root;

    int (*cmp_func)(void *d1, void *d2);

    atom_t counter;//trie node number
};

struct binary_node_list {
    struct binary_node *first;
    atom_t nodelist_counter;
};

struct binarytree_state {
    struct binary_node_list *nodelist;  //malloc trie nodes in batch
    struct binarytree *tree;
    atom_t init_node_counter;
};

struct binarytree_state_test {
    struct binarytree_state *trie_state;
    atom_t thread_num;
    atom_t read_line;
};

atom_t binarytree_new(struct binarytree_state **state, atom_t node_num);

atom_t binarytree_new_entry(struct binarytree **tree);

atom_t binarytree_new_nodelist(struct binary_node_list **nodelist, atom_t node_num);

atom_t binarytree_nodelist_new_elements(struct binary_node_list *nodelist, atom_t node_num);

void binarytree_use(struct binarytree_state *state);

void binarytree_node_push(struct binary_node_list *nodelist, struct binary_node *node);

atom_t binarytree_node_pop(struct binary_node_list *nodelist, struct binary_node **node);

// Self-operations, will balancing the tree in the future.
struct binary_node *
binarytree_get_subnode(struct binarytree_state *state, struct binary_node *parent, struct binary_node **prev,
                       struct binary_node **prev_next, void *data);

struct binary_node *binarytree_search_subnode(struct binarytree_state *state, struct binary_node *parent, void *data);

// Operations
struct BinaryTreeNode *
lf_binarytree_insert_node(struct binarytree_state *state, struct binary_node *parent, struct binary_node **prev,
                          struct binary_node **prev_next, void *data);

void lf_binarytree_state_free(struct binarytree_state *state);

void lf_binarytree_nodelist_free(struct binary_node_list *nodelist);

void lf_binarytree_free(struct binarytree *tree);

atom_t lf_binarytree_insert(struct binarytree_state *state, void *data);

atom_t lf_binarytree_search(struct binarytree_state *state, void *data);

// Default comparators.
int lf_binarytree_int_comparator(void *d1, void *d2);

#endif //LF_BINARYTREE_H
