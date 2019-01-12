#ifndef LF_MEMTRIE_H
#define LF_MEMTRIE_H
/* Comment: Will support deletion and update, and then upsert.
 * */

#include "lf_basic.h"
#include <string.h>

#define TRIE_INSERT_SUCCESS             1
#define TRIE_INSERT_DUPLICATION         2
#define TRIE_INSERT_ERROR               3
#define TRIE_SEARCH_SUCCESS             1
#define TRIE_SEARCH_ERROR               2
#define TRIE_NEW_NODELIST_NUM           200000000

struct trie_node {
    struct trie_node *volatile _firstSubNode;
    struct trie_node *volatile _brotherNode;
    unsigned char key;  //用于真实数据，模拟数据则atom_t key;
};

struct trie {
    struct trie_node *root;
    atom_t counter;//trie node number
};

struct trie_node_list {
    struct trie_node *first;
    atom_t nodelist_counter;
};

struct trie_state {
    struct trie_node_list *trieNodeList;  //malloc trie nodes in batch
    struct trie *trie_entry;
    atom_t init_node_counter;
};

struct trie_state_test {
    struct trie_state *trie_state;
    atom_t thread_num;
    atom_t read_line;
};

atom_t trie_new(struct trie_state **trie, atom_t trie_node_num);

atom_t trie_new_trie_entry(struct trie **trie);

atom_t trie_new_nodelist(struct trie_node_list **nodelist, atom_t trie_node_num);

atom_t trie_nodelist_new_elements(struct trie_node_list *nodelist, atom_t trie_node_num);

void tnode_push(struct trie_node_list *nodelist, struct trie_node *tnode);

atom_t tnode_pop(struct trie_node_list *nodelist, struct trie_node **tnode);

void trie_use(struct trie_state *ts);

//operation
atom_t lf_trie_insert_real_data(struct trie_state *trie_state, unsigned char *in_str, unsigned strlen);

atom_t lf_trie_search_real_data(struct trie_state *trie_state, unsigned char *in_str, unsigned strlen);

struct trie_node *
lf_trie_insert_node(struct trie_state *trie_state, struct trie_node *parrent, struct trie_node **prev,
                    struct trie_node **prev_next,
                    unsigned char key);

struct trie_node *
get_subnode_by_label(struct trie_node *parrent, struct trie_node **prev, struct trie_node **prev_next,
                     unsigned char key);

struct trie_node *search_subnode_by_label(struct trie_node *parrent, unsigned char key);

void trie_state_free(struct trie_state *trie_state);

void trie_nodelist_free(struct trie_node_list *nodelist);

void trie_entry_free(struct trie *trie);

#endif // LF_MEMTRIE_H
