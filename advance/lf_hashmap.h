#ifndef LF_HASHMAP_H
#define LF_HASHMAP_H

#include "lf_basic.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>

#pragma pack(push, LF_ALIGN_DOUBLE_POINTER)
#define MAX_HASH_LIST_NUM 30000000
#define MAX_HASH_DATA_NUM 30000000
#define HASH_MAP_NODE_EXIST    0
#define HASH_MAP_NODE_ADD      1
#define HASH_MAP_NODE_REPLACE  2
#define HEAD_NODE     1
#define I_NODE             2
#define END_NODE       3
#define HASH_MAP_SEARCH_FAIL   0
#define HASH_MAP_SEARCH_SUCC 1

struct hash_data {
    struct hash_data *volatile next;
    atom_t key;
    atom_t val;
};

struct hash_item {
    struct hash_item *volatile next;
    struct hash_data *data;
    int node_type;   //HEAD_NODE  1    I_NODE 2  END_NODE 3
};

struct data_list {
    struct hash_data *volatile hd;
    atom_t data_count;
};

struct hash {
    int bucket_max;
    unsigned long bucket_mask;
    struct hash_item **volatile table;
    struct hash_list *volatile hl;
    struct data_list *volatile dl;
};

struct hash_list {
    struct hash_item *volatile he;
    unsigned long item_num;
};
#pragma pack(pop)

//free memory
void hash_free(struct hash *h);

unsigned long lf_hash_delete(struct hash *h, unsigned long key);

void hashlist_delete(struct hash *h, void (*delete_func)(void *data, void *state), void *state);

void hashtable_delete(struct hash *h, void (*delete_func)(void *bucket, void *state), void *state);

void datalist_delete(struct hash *h, void (*delete_func)(void *data, void *state), void *state);

void hash_delete_item_func(void *bucket, void *state);

void hash_delete_data_func(void *data, void *state);

//new hash data and item批量初始化数据
atom_t hash_new(struct hash **h, int nbpow);

atom_t hash_new_element(struct hash_list **hl, atom_t new_num);

atom_t hashlist_internal_new_element(struct hash_list *hl, struct hash_item **he);

atom_t hash_new_data_list(struct data_list **dl, atom_t data_count);

atom_t hashlist_internal_new_data(struct data_list *dl, struct hash_data **hd, atom_t count);

//init datalist,hashlist,hashtable with adding at least one node because the CAS can't run with the null pointer;
int init_hash_table(struct hash *h, int bucket_max);

atom_t init_hashlist(struct hash_list *hl);

atom_t init_datalist(struct data_list *dl);

//lock free hash table add element ,the element is from datalist and hashlist
atom_t lf_hash_internal_new_data_from_datalist(struct hash *h, struct hash_data **data, atom_t *retry);

atom_t lf_hash_add_new_element_from_hashlist(struct hash *h, struct hash_item **h_item, atom_t *retry);


//operation on lock free hash map
atom_t lf_hash_add(struct hash *h, struct hash_data *hdata, atom_t *retry, atom_t *add_retry);

int lf_hash_search(struct hash *hs, struct hash_data *hd);

//data and item push and pop 已经批量产生
void datalist_push(struct data_list *dl, struct hash_data *hd);

void hashlist_push(struct hash_list *hl, struct hash_item *he);

struct hash_item *hashlist_pop(struct hash_list *hl, struct hash_item **he, atom_t *retry);

struct hash_data *datalist_pop(struct data_list *dl, struct hash_data **hd, atom_t *retry);


unsigned long hash_lookup(struct hash *h, unsigned long key);

void hashmap_use(struct hash *hs);

#endif // LF_HASHMAP_H
