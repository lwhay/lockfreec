//
// Created by lwh on 19-3-18.
//

#include <iostream>
#include "global.h"
#include "ycsb.h"
#include "tpcc.h"
#include "test.h"
#include "thread.h"
#include "manager.h"
#include "mem_alloc.h"
#include "query.h"
#include "plock.h"
#include "occ.h"
#include "vll.h"
#include "urcu_impl.h"

using namespace std;

thread_t **m_thds;

void parser(int argc, char *argv[]);

void *runner(void *id) {
    uint64_t tid = (uint64_t) id;
    m_thds[tid]->run();
    return nullptr;
}

int main(int argc, char *argv[]) {
    parser(argc, argv);
    cout << g_thr_pinning_policy << endl;
    cout << g_part_alloc << endl;
    cout << g_mem_pad << endl;
    cout << g_prt_lat_distr << endl;
    cout << g_part_cnt << endl;
    cout << g_virtual_part_cnt << endl;
    cout << g_thread_cnt << endl;
    cout << g_abort_penalty << endl;
    cout << g_central_man << endl;
    cout << g_ts_alloc << endl;
    cout << g_key_order << endl;
    cout << g_no_dl << endl;
    cout << g_timeout << endl;
    cout << g_dl_loop_detect << endl;
    cout << g_ts_batch_alloc << endl;
    cout << g_ts_batch_num << endl;
    mem_allocator.init(g_part_cnt, MEM_SIZE / g_part_cnt);
    stats.init();
    glob_manager = (Manager *) _mm_malloc(sizeof(Manager), 64);
    glob_manager->init();
    if (g_cc_alg == DL_DETECT) {
        dl_detector.init();
    }
    workload *m_wl;
    switch (WORKLOAD) {
        case YCSB:
            cout << "ycsb" << endl;
            m_wl = new ycsb_wl;
            break;
        case TPCC:
            cout << "tpcc" << endl;
            m_wl = new tpcc_wl;
            break;
        case TEST:
            cout << "test" << endl;
            m_wl = new TestWorkload;
            ((TestWorkload *) m_wl)->tick();
            break;
        default:
            assert(false);
    }
    m_wl->init();
    printf("workload initialized!\n");
    return 0;
}