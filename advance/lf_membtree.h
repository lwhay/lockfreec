#ifndef LF_BTREE_H
#define LF_BTREE_H

#include "lf_basic.h"

#define BTREE_INSERT_SUCCESS            1
#define BTREE_INSERT_DUPLICATION        2
#define BTREE_INSERT_ERROR              3
#define BTREE_SEARCH_SUCCESS            1
#define BTREE_SEARCH_ERROR              2
#define BTREE_NEW_NODELIST_NUM          200000000

template <KeyType>
struct BTreeNode {
    struct BTreeNode *volatile _firstChild;
    struct BTreeNode *volatile _botherNode;

    KeyType *volatile payload;
};

template <KeyType>
struct BTree {
    struct BTreeNode<KeyType> *_root;
    atom_t counter;
};

struct BTreeNodeList {
    struct BTreeNode *first;
    atom_t nodelist_counter;
};

struct BTreeState {
    struct BTreeNodeList *bTreeNodeList;  //malloc many trie node once
    struct BTree *btree_entry;
    atom_t init_node_counter;
};

struct BTreeStateTest {
    struct BTreeState *tree_state;
    atom_t thread_num;
    atom_t read_line;
};

atom_t btree_new(struct BTreeState **ptree, atom_t btree_node_num);

atom_t btree_new_entry(struct BTree **ptree);

atom_t btree_new_nodelist(struct BTreeNodeList **nodelist, atom_t btree_node_num);

atom_t btree_nodelist_new_elements(struct BTreeNodeList *nodelist, atom_t btree_node_num);

void btree_node_push(struct BTreeNodeList *nodelist, struct BTreeNode *node);

atom_t btree_node_pop(struct BTreeNodeList *nodelist, struct BTreeNode **node);

void btree_use(struct BTreeState *ts);

//operation
atom_t lf_btree_insert_real_data(struct BTreeState *btree_state, unsigned char *in_str, unsigned strlen);

atom_t lf_btree_search_real_data(struct BTreeState *btree_state, unsigned char *in_str, unsigned strlen);

struct BTreeNode *
lf_btree_insert_node(struct BTreeState *btree_state, struct BTreeNode *parrent, struct BTreeNode **prev,
                     struct BTreeNode **prev_next, unsigned char key);

struct BTreeNode *
getSubNodeByLabel(struct BTreeNode *parrent, struct BTreeNode **prev, struct BTreeNode **prev_next, unsigned char key);

struct BTreeNode *_searchGetSubNode(struct BTreeNode *parrent, unsigned char key);

void btree_state_free(struct BTreeState *btree_state);

void btree_nodelist_free(struct BTreeNodeList *nodelist);

void btree_entry_free(struct BTree *btree);

#endif // LF_BTREE_H
