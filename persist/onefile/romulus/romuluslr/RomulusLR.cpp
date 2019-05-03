//
// Created by Michael on 2019-05-04.
// The lockfree basis comes from https://github.com/pramalhe/OneFile for comparison purposes.
//

#include "RomulusLR.h"

namespace romuluslr {
    /*
     * <h1> RomulusLR </h1>
     * TODO: explain this...
     *
     */

// Global with the 'main' size. Used by pload()
    uint64_t g_main_size = 0;
// Global with the 'main' addr. Used by pload()
    uint8_t *g_main_addr = 0;

    uint8_t *g_main_addr_end;

    alignas(128) bool g_right = false;

    thread_local int tl_lrromulus = 0;
// Counter of nested write transactions
    thread_local int64_t tl_nested_write_trans = 0;
// Counter of nested read-only transactions
    thread_local int64_t tl_nested_read_trans = 0;

    RomulusLR gRomLR{};
    RomulusLR *romlr = nullptr;
}
