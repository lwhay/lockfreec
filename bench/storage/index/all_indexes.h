//
// Created by Michael on 2019-03-11.
//
#ifndef ALL_INDEXES_H
#define ALL_INDEXES_H

#include "config.h"

#if defined IDX_HASH
#   include "index_hash.h"
#else

#   include "index_adapter.h"

#endif

#endif /* ALL_INDEXES_H */
