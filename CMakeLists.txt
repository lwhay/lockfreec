cmake_minimum_required(VERSION 3.6)
project(lockfreec)
enable_language(CXX)
enable_language(C)
enable_language(ASM)
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    #set(BOOST_ROOT D:/cygwin64)
    #set(Boost_INCLUDE_DIR ${BOOST_ROOT}/usr/include)
    #set(Boost_LIBRARY_DIR ${BOOST_ROOT}/lib)
    #find_package(Boost 1.66.0 REQUIRED COMPONENTS date_time filesystem iostreams)
    find_package(Boost 1.50.0 REQUIRED COMPONENTS date_time filesystem iostreams)
    include_directories(${Boost_INCLUDE_DIR})
    link_directories(${Boost_LIBRARY_DIR})
endif ()

include_directories(.)
include_directories(advance)
include_directories(advance/hashfunc)
include_directories(bench)
include_directories(bench/cc)
include_directories(bench/common)
include_directories(bench/misc)
include_directories(bench/loads)
include_directories(bench/storage)
include_directories(bench/storage/index)
include_directories(bench/stm)
include_directories(bench/stm/rlu)
include_directories(bench/stm/urcu)
include_directories(bench/stm/papi)
include_directories(bench/stm/recordmgr)
include_directories(bench/stm/atomic_ops)
include_directories(bench/stm/atomic_ops/atomic_ops)
include_directories(bench/stm/atomic_ops/sysdeps)
include_directories(bench/stm/brown)
include_directories(bench/stm/brown/abtree_lf)
include_directories(bench/stm/brown/abtree_brown_rq_lf)
include_directories(bench/stm/brown/bslack_brown_rq_lf)
include_directories(bench/stm/brown/bst_brown_rq_lf)
include_directories(bench/stm/brown/bst_brown_rwlock)
include_directories(bench/stm/brown/bst_brown_glock)
include_directories(bench/stm/brown/bst_brown_hohlock)
include_directories(bench/stm/brown/bst_brown_hohrwlock)
include_directories(bench/stm/brown/bst_brownson_occ)
include_directories(bench/stm/brown/bst_drach_lock)
include_directories(bench/stm/brown/bst_ellen_lf)
include_directories(bench/stm/brown/bst_guer_ticket)
include_directories(bench/stm/brown/bst_howley_lf)
include_directories(bench/stm/brown/bst_natarajan_lf)
include_directories(bench/stm/brown/bst_rama_lf)
include_directories(bench/stm/descriptors)
include_directories(bench/stm/dcss)
include_directories(bench/stm/rq)
include_directories(bench/stm/rq/snapcollector)
include_directories(basic)
include_directories(cases)
include_directories(mwcas)
include_directories(mwcas/harris)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(PTHREAD -lpthread)
    set(MLIB -lm)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimplicit-function-declaration")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wincompatible-pointer-types -Wint-conversion")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimplicit-function-declaration -Wdiscarded-qualifiers")
endif ()

set(LF_LIBS
        advance/lf_hashmap.c
        advance/lf_memtrie.c
        advance/lf_binarytree.c
        basic/internal.c
        basic/lf_basic.c
        basic/lf_freelist.c
        basic/lf_queue.c
        basic/lf_ringbuffer.c
        basic/lf_slist.c
        basic/lf_stack.c
        cases/abstraction.c
        cases/ut_freelist.c
        cases/ut_hashmap.c
        cases/ut_memtrie.c
        cases/ut_queue.c
        cases/ut_ringbuffer.c
        cases/ut_slist.c
        cases/ut_stack.c)

set(MWLF_LIBS
        mwcas/rel_ptr.h
        mwcas/pseudoio.h
        mwcas/mmwcas.h
        mwcas/ebr.h
        mwcas/gc.h)

enable_testing()
option(WITH_UNIT_TESTS "build with unittest" ON)
if (WITH_UNIT_TESTS)
    set(UNIT_TEST
            tests/unittests/basic_hashtest.cpp
            tests/unittests/bloom_bitsettest.c
            tests/unittests/bloom_hashtest.c)
endif ()

option(WITH_CC_DBX1K "build with concurrent control" ON)
if (WITH_CC_DBX1K)
    set(CC_DB1K_LIBS
            bench/config.cpp
            bench/misc/thread_pinning.cpp
            bench/misc/query.cpp
            bench/misc/wl.cpp
            bench/misc/thread.cpp
            bench/misc/parser.cpp
            bench/misc/helper.cpp
            bench/misc/global.cpp
            bench/misc/manager.cpp
            bench/misc/stats.cpp
            bench/misc/txn.cpp
            bench/common/mem_alloc.cpp
            bench/cc/vll.cpp
            bench/cc/row_lock.cpp
            bench/cc/dl_detect.cpp
            bench/loads/tpcc_txn.cpp
            bench/loads/tpcc_helper.cpp
            bench/loads/tpcc_query.cpp
            bench/loads/tpcc_wl.cpp
            bench/loads/ycsb_query.cpp
            bench/loads/ycsb_wl.cpp
            bench/loads/ycsb_txn.cpp
            bench/loads/test_txn.cpp
            bench/storage/catalog.cpp
            bench/storage/row.cpp
            bench/storage/table.cpp
            bench/storage/index/index_hash.cpp)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "CYGWIN")
    message("TM built on ${CMAKE_SYSTEM_NAME}")
    set(DL "-ldl")
    set(JEMALLOC "-ljemalloc")
    set(URCU "-L../libs -lurcu")
    set(PAPI -lpapi)
    option(WITH_BROWN_LF "build with brown lockfree structures" ON)
    if (WITH_BROWN_LF)
        set(BROWN_IF_STRUCT
                bench/stm/rlu/rlu.cpp
                #bench/stm/brown/abtree_lf/brown_abtree_lf_example.cpp)
                )
    endif ()
    option(WITH_DBX1K_TESTS "build with concurrent control" ON)
    if (WITH_DBX1K_TESTS)
        set(workload TPCC)
        set(cpu_freq_ghz 2.1)
        set(CMAKE_CXX_FLAGS "-DNOGRAPHITE=1 -DWORKLOAD=${workload} ${CMAKE_CXX_FLAGS}")
        set(CMAKE_CXX_FLAGS "-DUSE_API -DCPU_FREQ_GHZ=${cpu_freq_ghz} -DCPU_FREQ=${cpu_freq_ghz} ${CMAKE_CXX_FLAGS}")
        set(DBX1K_TESTS
                bench/db1korigin.cpp
                bench/db1ktest.cpp
                )
        set(BROWN_TESTS
                bench/stm/brown/abtree_brown_lf/brown_abtree_lf_example.cpp
                bench/stm/brown/abtree_brown_lf/brown_abtree_lf_perf.cpp
                bench/stm/brown/abtree_brown_rq_lf/brown_abtree_rq_lf_example.cpp
                bench/stm/brown/abtree_brown_rq_lf/brown_abtree_rq_lf_perf.cpp
                bench/stm/brown/bslack_brown_rq_lf/brown_bslack_example.cpp
                bench/stm/brown/bst_brown_ist_lf/brown_ist_lf_perf.cpp
                bench/stm/brown/bst_brown_rq_lf/brown_bst_rq_lf_example.cpp
                bench/stm/brown/bst_brown_rq_lf/brown_bst_rq_lf_perf.cpp
                bench/stm/brown/bst_brown_rwlock/brown_bst_rwlock_perf.cpp
                bench/stm/brown/bst_brown_glock/brown_bst_glock_perf.cpp
                bench/stm/brown/bst_brown_hohlock/brown_bst_hohlock_perf.cpp
                bench/stm/brown/bst_brown_hohrwlock/brown_bst_horwhlock_perf.cpp
                bench/stm/brown/bst_brownson_occ/brownson_bst_occ_perf.cpp
                bench/stm/brown/bst_drach_lock/bst_drash_lock_perf.cpp
                bench/stm/brown/bst_ellen_lf/bst_ellen_lf_perf.cpp
                bench/stm/brown/bst_guer_ticket/bst_guer_ticket_perf.cpp
                bench/stm/brown/bst_howley_lf/bst_howley_lf_perf.cpp
                bench/stm/brown/bst_natarajan_lf/bst_natarajan_lf_perf.cpp
                bench/stm/brown/bst_rama_lf/bst_rama_lf_perf.cpp
                #bench/stm/rq/snapcollector/snapcollector_test.cpp
                )
    endif ()
endif ()

option(WITH_LF_TESTS "build with lftests" ON)
if (WITH_LF_TESTS)
    set(LF_TEST
            tests/it_lockfree_all.c
            tests/it_dcastest.c
            tests/it_binarytree.c)
endif ()

option(WITH_MWLF_TESTS "build with multiword lftests" ON)
if (WITH_MWLF_TESTS)
    set(MWLF_TEST
            tests/mwctests/harris/datatypetest.c)
    set(MWLF_TEST
            tests/mwctests/harris/valcastest.cpp
            tests/it_moodycqtest.cpp
            tests/it_basiclfqueuetest.cpp
            ${MWLF_TEST})
    if (NOT (CMAKE_SYSTEM_NAME MATCHES "Linux"))
        set(MWLF_TEST
                tests/it_mwcastest.cpp
                tests/mwctests/utebrtest.cpp
                ${MWLF_TEST})
    endif ()
endif ()

find_package(Threads)

option(WITH_BOOST_TESTS "build with tests" ON)
if (WITH_BOOST_TESTS)
    set(BOOST_TESTS
            compare/boost/boostlockfree.cpp
            )
endif ()

option(WITH_FOSTER_TESTS "build with foster tests" ON)
if (WITH_FOSTER_TESTS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSTANDALONE")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dlinux")
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(FOSTER_TESTS
                sandbox/btree2s.c
                sandbox/btree2t.c
                sandbox/btree2u.c
                sandbox/btree2v.c
                sandbox/threads2h.c
                sandbox/threads2i.c
                sandbox/threads2j.c
                sandbox/threadskv8.c
                #sandbox/threadskv10g.c
                #sandbox/threadskv10h.c
                sandbox/btree2sTest.c
                sandbox/btree2sBulkInsert.c
                )
    else ()
        set(FOSTER_TESTS
                sandbox/btree2s.c
                sandbox/btree2t.c
                sandbox/btree2u.c
                sandbox/threads2h.c
                sandbox/threads2i.c
                sandbox/threadskv8.c
                #sandbox/threadskv10g.c
                #sandbox/threadskv10h.c
                sandbox/btree2sTest.c
                sandbox/btree2sBulkInsert.c
                )
    endif ()
endif ()

set(UNIT_TESTS_EXES ${UNIT_TEST})
foreach (sourcefile ${UNIT_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD} ${MLIB})
endforeach (sourcefile ${UNIT_TESTS_EXES})

set(LF_TESTS_EXES ${LF_TEST})
foreach (sourcefile ${LF_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile} ${LF_LIBS})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD})
endforeach (sourcefile ${LF_TESTS_EXES})

set(MWLF_TESTS_EXES ${MWLF_TEST})
foreach (sourcefile ${MWLF_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile} ${MWLF_LIBS})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD})
endforeach (sourcefile ${MWLF_TESTS_EXES})

set(DBX1K_TESTS_EXES ${DBX1K_TESTS})
foreach (sourcefile ${DBX1K_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile} ${CC_DB1K_LIBS} ${BROWN_IF_STRUCT})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD} ${DL} ${JEMALLOC} ${URCU} ${PAPI})
endforeach (sourcefile ${DBX1K_TESTS_EXES})

set(BROWN_TESTS_EXES ${BROWN_TESTS})
foreach (sourcefile ${BROWN_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD})
endforeach (sourcefile ${DBX1K_TESTS_EXES})

set(BOOST_TESTS_EXES ${BOOST_TESTS})
foreach (sourcefile ${BOOST_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${Boost_LIBRARIES} ${PTHREAD})
    #    if (NOT "${exename}" MATCHES "db_sanity_test")
    #        add_test(NAME ${exename} COMMAND ${exename}${ARTIFACT_SUFFIX})
    #        add_dependencies(check ${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX})
    #    endif ()
endforeach (sourcefile ${BOOST_TESTS})

set(FOSTER_TESTS_EXES ${FOSTER_TESTS})
foreach (sourcefile ${FOSTER_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile} sandbox/btrees.h)
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD})
endforeach (sourcefile ${FOSTER_TESTS_EXES})
