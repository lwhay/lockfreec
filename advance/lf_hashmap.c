#include "lf_hashmap.h"

atom_t hash_new(struct hash **h, int nbpow) {
    int bucket_max = ((unsigned long) 1 << nbpow);
    unsigned long bucket_mask = (unsigned long) bucket_max - 1;
    *h = (struct hash *) aligned_malloc(sizeof(struct hash), LF_ALIGN_DOUBLE_POINTER);
    if (*h != NULL) {
        if (!init_hash_table(*h, bucket_max))
            return 0;
        hash_new_element(&(*h)->hl, MAX_HASH_LIST_NUM);
        hash_new_data_list(&(*h)->dl, MAX_HASH_DATA_NUM);
    }
    if (!(*h && (*h)->table && (*h)->hl && (*h)->dl)) {
        if (*h) {
            if ((*h)->hl) {
                hashlist_delete(*h, hash_delete_item_func, NULL);
                aligned_free((*h)->hl);
            }
            if ((*h)->dl) {
                datalist_delete(*h, hash_delete_data_func, NULL);
                aligned_free((*h)->dl);
            }
            aligned_free(*h);
        }
        return 0;
    }
    (*h)->bucket_max = bucket_max;
    (*h)->bucket_mask = bucket_mask;
    LF_BARRIER_STORE;
    return 1;
}

int init_hash_table(struct hash *h, int bucket_max) {
    assert(h != NULL);
    struct hash_item *head, *end;
    atom_t loop, count = 0;
    h->table = (struct hash_item **) aligned_malloc(sizeof(struct hash_item *) * bucket_max, LF_ALIGN_DOUBLE_POINTER);
    for (loop = 0; loop < bucket_max; loop++) {
        head = (struct hash_item *) aligned_malloc(sizeof(struct hash_item), LF_ALIGN_DOUBLE_POINTER);
        end = (struct hash_item *) aligned_malloc(sizeof(struct hash_item), LF_ALIGN_DOUBLE_POINTER);
        if (head && end) {
            head->node_type = HEAD_NODE;
            end->node_type = END_NODE;
            h->table[loop] = head;
            h->table[loop]->next = end;
        } else {
            return 0;
        }
    }
    return 1;
}

/*------------------aligned free memory -----------*/
void hash_free(struct hash *h) {
    if (h != NULL) {
        if (h->hl != NULL) {
            hashlist_delete(h, hash_delete_item_func, NULL);
            aligned_free(h->hl);
        }

        if (h->dl != NULL) {
            datalist_delete(h, hash_delete_data_func, NULL);
            aligned_free(h->dl);
        }

        if (h->table != NULL) {
//#ifndef __MINGW64__
            //Now we have a bug in mingw64.
            printf("free test\n");
            hashtable_delete(h, hash_delete_item_func, NULL);
            printf("free test suc\n");
//#endif
            aligned_free(*(h->table));
        }

        aligned_free(h);
    }
    return;
}

/*   hash delete will be completed soon */
void hashlist_delete(struct hash *h, void (*delete_func)(void *data_item, void *state), void *state) {
    assert(h->hl != NULL);
    assert(state == NULL);
    assert(delete_func != NULL);

    struct hash_item *ihash;
    int retry = 0;

    while (hashlist_pop(h->hl, &ihash, &retry)) {
        delete_func(ihash, state);
    }

    return;
}

void hashtable_delete(struct hash *h, void (*delete_func)(void *bucket, void *state), void *state) {
    assert(delete_func != NULL);
    assert(state == NULL);

    int bucket_max = h->bucket_max;
    struct hash_item *bucket, *next;
    int i;

    for (i = 0; i < bucket_max; i++) {
        bucket = h->table[i];
        while (bucket) {
            next = bucket->next;
            delete_func(bucket, state);
            bucket = next;
        }
    }

    return;
}

void datalist_delete(struct hash *h, void (*delete_func)(void *data, void *state), void *state) {
    assert(h->dl != NULL);
    assert(state == NULL);
    assert(delete_func != NULL);

    struct hash_data *hd;
    int retry = 0;

    while (datalist_pop(h->dl, &hd, &retry)) {
        delete_func(hd, state);
    }
    return;

}

void hash_delete_item_func(void *bucket, void *state) {
    assert(bucket != NULL);
    assert(state == NULL);

    struct hash_item *_bucket = (struct hash_item *) bucket;

    if (_bucket->data != NULL)
        aligned_free(_bucket->data);
    aligned_free(bucket);

    return;
}

void hash_delete_data_func(void *data, void *state) {
    assert(data != NULL);
    assert(state == NULL);

    aligned_free(data);

    return;
}

/*---------------------aligned free memory over----------*/

atom_t hash_new_element(struct hash_list **hl, atom_t number_elements) {
    assert(hl != NULL);

    atom_t rv = 0;
    struct hash_item *he;
    atom_t loop, count = 0;
    *hl = (struct hash_list *) aligned_malloc(sizeof(struct hash_list), LF_ALIGN_DOUBLE_POINTER);

    if (!init_hashlist(*hl))
        return 0;

    for (loop = 0; loop < number_elements; loop++) {
        if (hashlist_internal_new_element(*hl, &he)) {
            hashlist_push(*hl, he);
            count++;
        }
    }

    if (number_elements == count) {
        rv = 1;
    } else {
        aligned_free(*hl);
        *hl = NULL;
    }

    internal_display_test_name("new item list over\n");

    return rv;
}

atom_t init_hashlist(struct hash_list *hl) {
    assert(hl != NULL);

    atom_t rv = 0;
    hl->he = (struct hash_item *) aligned_malloc(sizeof(struct hash_item), LF_ALIGN_DOUBLE_POINTER);

    if (hl->he) {
        hl->item_num = 1;
        rv = 1;
    }

    return rv;
}

atom_t hashlist_internal_new_element(struct hash_list *hl, struct hash_item **he) {
    assert(hl != NULL);

    atom_t rv = 0;
    *he = (struct hash_item *) aligned_malloc(sizeof(struct hash_item), LF_ALIGN_DOUBLE_POINTER);

    if (NULL != *he) {
        abstraction_increment((atom_t *) &hl->item_num);
        (*he)->node_type = I_NODE;
        rv = 1;
    }

    return (rv);
}


void hashlist_push(struct hash_list *hl, struct hash_item *he) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_item *he_local, *he_original;
    he_local = he;
    LF_BARRIER_LOAD;

    he_original = hl->he;

    do {
        he_local->next = he_original;
    } while (he_local->next !=
             (he_original = (struct hash_item *) abstraction_cas((volatile atom_t *) &hl->he, (atom_t *) he_local,
                                                                 (atom_t *) he_original)));
    return;
}

struct hash_item *hashlist_pop(struct hash_list *hl, struct hash_item **he, atom_t *retry) {
    assert(NULL != hl);
    assert(NULL != he);

    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_item *he_local, *he_cmp;
    LF_BARRIER_LOAD;

    he_local = hl->he;
    do {
        he_cmp = he_local;
        if (he_local == NULL || he_local->next == NULL) {
            *he = NULL;
            return (*he);
        }
        (*retry)++;
    } while (he_cmp !=
             (he_local = abstraction_cas((volatile atom_t *) &hl->he, (atom_t) he_local->next, (atom_t) he_cmp)));

    abstraction_sub((atom_t *) &hl->item_num);
    *he = (struct hash_item *) he_local;

    return (*he);
}


/*-------------malloc many data node to a data list,about pop and push-------------*/
atom_t hash_new_data_list(struct data_list **dl, atom_t data_count) {
    assert(dl != NULL);
    atom_t rv = 0;
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_data *hd;
    atom_t loop, count = 0;
    *dl = (struct data_list *) aligned_malloc(sizeof(struct data_list), LF_ALIGN_DOUBLE_POINTER);
    if (!init_datalist(*dl))
        return 0;
    for (loop = 0; loop < data_count; loop++)
        if (hashlist_internal_new_data(*dl, &hd, count)) {
            hd->val = count;
            datalist_push(*dl, hd);
            count++;
        }

    if (data_count == count) {
        rv = 1;
    } else {
        aligned_free(*dl);
        *dl = NULL;
    }
    internal_display_test_name("new data list over...");
    printf("%u\n", count);
    fflush(stdout);
    return rv;
}

atom_t init_datalist(struct data_list *dl) {
    assert(dl != NULL);
    atom_t rv = 0;
    dl->hd = (struct hash_data *) aligned_malloc(sizeof(struct hash_data), LF_ALIGN_DOUBLE_POINTER);
    if (dl->hd) {
        dl->data_count = 1;
        rv = 1;
    }
    return rv;
}

atom_t hashlist_internal_new_data(struct data_list *dl, struct hash_data **hd, atom_t count) {
    atom_t rv = 0;
    assert(dl != NULL);

    *hd = (struct hash_data *) aligned_malloc(sizeof(struct hash_data), LF_ALIGN_DOUBLE_POINTER);
    if (NULL != hd) {
        (*hd)->key = count;
        (*hd)->val = count;
        abstraction_increment((atom_t *) &dl->data_count);
        rv = 1;
    }
    LF_BARRIER_STORE;
    return (rv);
}

void datalist_push(struct data_list *dl, struct hash_data *hd) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_data *hd_local, *hd_original;
    hd_local = hd;
    LF_BARRIER_LOAD;
    hd_original = dl->hd;
    do {
        hd_local->next = hd_original;
    } while (hd_local->next !=
             (hd_original = (struct hash_data *) abstraction_cas((volatile atom_t *) &dl->hd, (atom_t) hd_local,
                                                                 (atom_t) hd_original)));
    // internal_display_test_name("datalist push over...\n");
    return;
}

struct hash_data *datalist_pop(struct data_list *dl, struct hash_data **hd, atom_t *retry) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_item *hd_local, *hd_cmp;
    assert(NULL != dl);
    //assert(NULL == (*hd));
    //internal_display_test_name("hashmap datalist pop...");
    LF_BARRIER_LOAD;
    hd_local = dl->hd;
    do {
        hd_cmp = hd_local;
        if (hd_local->next == NULL) {
            *hd = NULL;
            return (*hd);
        }
        (*retry)++;
    } while (hd_cmp !=
             (hd_local = (struct hash_item *) abstraction_cas((volatile atom_t *) &dl->hd, (atom_t) hd_local->next,
                                                              (atom_t) hd_cmp)));
    abstraction_sub((atom_t *) &dl->data_count);
    *hd = (struct hash_data *) hd_local;
    return (*hd);
}


unsigned long hash_lookup(struct hash *h, unsigned long key) {
    unsigned long bucket_mask = h->bucket_max;
    int bucket_index = (int) (key & bucket_mask);
    struct hash_item *bucket;
    unsigned long val;
    for (bucket = h->table[bucket_index]; bucket; bucket = bucket->next)
        if (bucket->data->key == key) {
            val = bucket->data->val;
            return val;
        }
    return 0;
}
/*
unsigned long hash_insert(struct hash* h, unsigned long key, unsigned long val) {
    unsigned long bucket_mask = h->bucket_mask;
    int bucket_index = (int)(key & bucket_mask);
    struct hash_item* new_item;
    new_item = (struct hash_item*)malloc(sizeof(struct hash_item));
    if (!new_item) {
        return -1;
    }
    new_item->key = key;
    new_item->value = val;
    new_item->next = h->table[bucket_index];
    h->table[bucket_index] = new_item;
    return 0;
}

unsigned long hash_delete(struct hash* h, unsigned long key) {
    unsigned long bucket_mask = h->bucket_mask;
    int bucket_index = (int)(key & bucket_mask);
    struct hash_item *bucket, *prev;
    unsigned long val;
    for (prev = NULL, bucket = h->table[bucket_index]; bucket; prev = bucket, bucket = bucket->next) {
        if (bucket->key == key) {
            val = bucket->value;
            if (prev)
                prev->next = bucket->next;
            else
                h->table[bucket_index] = bucket->next;
            free(bucket);
            return val;
        }
    }
    return 0;
}*/
/*-----------------------     add node or replace node or node exist   ------------------ */
atom_t lf_hash_add(struct hash *h, struct hash_data *hdata, atom_t *retry, atom_t *add_retry) {
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_item *h_item, *bucket, *h_prev, *prev_next;
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_data *h_data, *hd_cmp;
    assert(h != NULL);
    unsigned long bucket_mask = h->bucket_mask;
    int bucket_index = (int) (hdata->key & bucket_mask);
    atom_t operation, cas_result = 0;
    //internal_display_test_name("hashmap add start...");
    while (0 == lf_hash_add_new_element_from_hashlist(h, &h_item, retry));
    h_prev = bucket = h->table[bucket_index];
    //test
    prev_next = h_prev->next;
    h_item->data = hdata;
    assert(h_item != NULL);
    LF_BARRIER_LOAD;
//todo   按照修改双索引的方式插入;
    do {
        if (prev_next->node_type == END_NODE || prev_next->data->key > hdata->key) {
            h_item->next = prev_next;
            bucket = prev_next;
            prev_next = (struct hash_item *) abstraction_cas((volatile atom_t *) &h_prev->next, (atom_t) h_item,
                                                             (atom_t) bucket);
            if (prev_next == bucket) {
                operation = HASH_MAP_NODE_ADD;
                cas_result = 1;
            } else {
                //exchange index error;
                cas_result = 0;
            }
        } else if (prev_next->data->key < hdata->key) {
            h_prev = prev_next;
            prev_next = prev_next->next;
        } else {
            if (hdata->val == prev_next->data->val) {
                aligned_free(hdata);
                operation = HASH_MAP_NODE_EXIST;
            } else {
                hd_cmp = prev_next->data;
                do {
                    h_data = hd_cmp;
                } while (h_data != (hd_cmp = (struct hash_data *) abstraction_cas((volatile atom_t *) &prev_next->data,
                                                                                  (atom_t) hdata, (atom_t) h_data)));
                aligned_free(h_data);
                operation = HASH_MAP_NODE_REPLACE;
            }
            cas_result = 1;
        }
        (*add_retry)++;
    } while (cas_result == 0);
    if (operation == HASH_MAP_NODE_EXIST || operation == HASH_MAP_NODE_REPLACE) {
        aligned_free(h_item);
    }
    return operation;
}

int lf_hash_search(struct hash *hs, struct hash_data *hd) {
    assert(hs != NULL);
    assert(hd != NULL);
    int cas_result = 0, operation;
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_item *bucket, *h_prev;
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_data *h_data;
    unsigned long bucket_mask = hs->bucket_mask;
    int bucket_index = (int) (hd->key & bucket_mask);
    //internal_display_test_name("hashmap add start...");
    h_prev = hs->table[bucket_index];
    //test
    bucket = h_prev->next;
    LF_BARRIER_LOAD;
//todo   查找索引
    do {
        if (bucket->node_type == END_NODE || bucket->data->key > hd->key) {
            cas_result = 1;
            operation = HASH_MAP_SEARCH_FAIL;
        } else if (bucket->data->key < hd->key) {
            h_prev = bucket;
            bucket = bucket->next;
        } else {
            operation = HASH_MAP_SEARCH_SUCC;
            cas_result = 1;
        }
    } while (cas_result == 0);
    return operation;
}

atom_t lf_hash_add_new_element_from_hashlist(struct hash *h, struct hash_item **h_item, atom_t *retry) {
    //todo get a hash_item from hash_list
    atom_t rv = 0;
    assert(h != NULL);
    assert(h_item != NULL);
    hashlist_pop(h->hl, h_item, retry);
    if (*h_item != NULL) {
        rv = 1;
    }
    return rv;
}

atom_t lf_hash_internal_new_data_from_datalist(struct hash *h, struct hash_data **data, atom_t *retry) {
    atom_t rv = 0;
    assert(h != NULL);
    assert(data != NULL);
    datalist_pop(h->dl, data, retry);
    if (*data != NULL) {
        rv = 1;
    }
    return rv;
}

/* -------------------add over ------------------*/
#pragma warning(disable : 4100)

void hashmap_use(struct hash *hs) {
    assert (NULL != hs);
    LF_BARRIER_LOAD;
    return;
}

//#pragma warning(default : 4100)

#pragma warning(disabe : 4100)
/*------------------------------ hash remove item ------------------------*/
//todo
/*
atom_t hash_remove_item(struct hash * h, atom_t key)
{
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_item *h_item,*bucket,*h_prev;
    LF_ALIGN(LF_ALIGN_DOUBLE_POINTER) struct hash_data *h_data;
    assert(h != NULL);
    unsigned long bucket_mask = h->bucket_mask;
    int bucket_index = (int)(hdata->key & bucket_mask);
    atom_t operation,cas_result = 0;
    h_prev = bucket = h->table[bucket_index];
    bucket = h_prev->next;
    do {
        if(bucket == NULL) {

            break;
        }
        if(bucket->data->key == key)
        {
            cas_result = abstraction_dcas((volatile atom_t*)h_prev->next,(atom_t*)bucket->next,(atom_t*)bucket);
        }

    } while (cas_result == 0);
}
*/
unsigned long lf_hash_delete(struct hash *h, unsigned long key) {

}
