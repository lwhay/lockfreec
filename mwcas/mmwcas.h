//
// Created by Michael on 2018/12/18.
//

#ifndef LOCKFREEC_MMWCAS_H
#define LOCKFREEC_MMWCAS_H

#include "gc.h"
#include "rel_ptr.h"
#include "pseudoio.h"

#ifdef IS_PMEM
#define persist     pmem_persist
#else
#define persist     mmem_persist
#endif

#define RDCSS_BIT                       0x8000000000000000
#define MwCAS_BIT                       0x4000000000000000
#define DIRTY_BIT                       0x2000000000000000
#define ADDR_MASK                       0xffffffffffff

#define ST_UNDECIDED                    0
#define ST_SUCCESS                      1
#define ST_FAILED                       2
#define ST_FREE                         3

#define RELEASE_NEW_ON_FAILED           1
#define RELEASE_EXP_ON_SUCCESS          2
#define RELEASE_SWAP_PTR                3
#define NOCAS_RELEASE_ADDR_ON_SUCCESS   4
#define NOCAS_EXECUTE_ON_FAILED         5
#define NOCAS_RELEASE_NEW_ON_FAILED     6

struct word_entry;
struct mmwcas_entry;
struct mmwcas_pool;

typedef rel_ptr<word_entry> wdesc_t;
typedef rel_ptr<mmwcas_entry> mdesc_t;
typedef mmwcas_pool *mdesc_pool_t;

struct word_entry {
    rel_ptr<uint64_t> add;
    uint64_t expect;
    uint64_t new_val;
    wdesc_t mdesc;
    off_t recycle_func; /* 0: non-operation, 1: free new_val when fails, 2: free expect when successes*/
};

struct mmwcas_entry {
    uint64_t status;
    gc_entry_t gc_entry;
    mdesc_pool_t mdesc_pool;
    size_t count;
    off_t callback;
    word_entry wdescs[WORD_DESCRIPTOR_SIZE];
};

#endif //LOCKFREEC_MMWCAS_H
