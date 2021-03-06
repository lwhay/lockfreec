cmake_minimum_required(VERSION 3.6)
project(lockfreec)
enable_language(CXX)
enable_language(C)
enable_language(ASM)
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_C_STANDARD 11)
if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.1)
    set(CMAKE_CXX_STANDARD 14)
else ()
    set(CMAKE_CXX_STANDARD 11)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    #set(BOOST_ROOT D:/cygwin64)
    #set(Boost_INCLUDE_DIR ${BOOST_ROOT}/usr/include)
    #set(Boost_LIBRARY_DIR ${BOOST_ROOT}/lib)
    #find_package(Boost 1.66.0 REQUIRED COMPONENTS date_time filesystem iostreams)
    find_package(Boost 1.50.0 REQUIRED COMPONENTS date_time filesystem iostreams)
    include_directories(${Boost_INCLUDE_DIR})
    link_directories(${Boost_LIBRARY_DIR})
    link_directories(/usr/local/lib)
endif ()

if (NOT CMAKE_SYSTEM_NAME MATCHES "CYGWIN")
    find_package(tbb 1.0 REQUIRED COMPONENTS tbbmalloc tbb tbb_preview)
    if (NOT TBB_FIND_COMPONENTS EQUAL 0)
        message("-- Found TBB at: ${TBB_DIR} ${TBB_ROOT}")
        message("--   ${TBB_FIND_COMPONENTS} found at ${TBB_ROOT}")
        set(LTBB "-L${TBB_DIR}/../../ -ltbb")
        set(LTBBMALLOC "-L${TBB_DIR}/../../ -ltbbmalloc")
    endif ()
endif ()

include_directories(.)
include_directories(advance)
include_directories(advance/hashfunc)
include_directories(memory)
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
include_directories(misc)
include_directories(persist)
include_directories(persist/pqueue)
include_directories(persist/onefile)
include_directories(hash/mhash)
include_directories(btree)
include_directories(btree/bwtree)

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

if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_definitions(-DBWTREE_NODEBUG=1)
endif ()

set(BWTREE_LIBS
        btree/bwtree/bwtree.cpp)

set(BWTREE_TESTS
        tests/btree/bwtree_unittest.cpp)

enable_testing()
option(WITH_UNIT_TESTS "build with unittest" ON)
if (WITH_UNIT_TESTS)
    set(UNIT_TEST
            tests/unittests/basic_hashtest.cpp
            tests/unittests/basic_hashmapcomp.cpp
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
            bench/cc/row_mvcc.cpp
            bench/cc/row_occ.cpp
            bench/cc/row_ts.cpp
            bench/cc/row_hekaton.cpp
            bench/cc/row_silo.cpp
            bench/cc/row_tictoc.cpp
            bench/cc/row_vll.cpp
            bench/cc/occ.cpp
            bench/cc/ticktoc.cpp
            bench/cc/silo.cpp
            bench/cc/hekaton.cpp
            bench/cc/plock.cpp
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
    execute_process(COMMAND jemalloc-config --libdir
            OUTPUT_VARIABLE JEMALLOC_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(JEMALLOC "-L${JEMALLOC_LIBDIR} -ljemalloc")
    message(${JEMALLOC})
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
        set(workload YCSB)
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
                bench/stm/brown/abtree_brown_kcas/brown_abtree_kcas_perf.cpp
                bench/stm/brown/bslack_brown_rq_lf/brown_bslack_example.cpp
                bench/stm/brown/bslack_brown_rq_lf/brown_bslack_perf.cpp
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
            tests/platform/floatCompTest.cpp
            tests/platform/stdcasTest.cpp
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

option(WITH_LFQ_TESTS "build with tests" ON)
if (WITH_LFQ_TESTS)
    set(LFQ_TESTS
            compare/boostlockfree.cpp
            tests/hash/MH_MicroTest.cpp
            )
    if (NOT CMAKE_SYSTEM_NAME MATCHES "CYGWIN")
        set(LFQ_TESTS
                compare/tbblockfree.cpp
                ${LFQ_TESTS})
    endif ()
endif ()

message("-- GCC Version ${CMAKE_CXX_COMPILER_VERSION}")
if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.1)
    option(WITH_PLF_TESTS "build with persistent lf tests" ON)
else ()
    option(WITH_PLF_TESTS "disable persistent lf tests" OFF)
endif ()

if (WITH_PLF_TESTS)
    set(CMAKE_CXX_FLAGS "-DPWB_IS_CLFLUSHHARD ${CMAKE_CXX_FLAGS}")
    set(PLF_TESTS
            tests/persisttests/pqueue/MichaelScottQueueTest.cpp
            tests/persisttests/pqueue/PMichaelScottQueueTest.cpp
            tests/persisttests/onefile/OneFileDCASTest.cpp
            tests/persisttests/onefile/OneFileHashSetLFTest.cpp
            tests/persisttests/onefile/OneFileHashSetWFTest.cpp
            tests/persisttests/onefile/OneFilePHashSetLFTest.cpp
            tests/persisttests/onefile/OneFilePHashSetWFTest.cpp
            )
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.1 AND CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(PMDK_LIBS "-L/usr/local/lib -lpmemobj")
        set(PLF_TESTS
                tests/persisttests/onefile/OneFilePMDKHashSetTest.cpp
                ${PLF_TESTS})
    endif ()
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

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    option(MEM_COMP_TESTS "compare jemalloc libc supermalloc" ON)

    set(SUPER_MALLOC_LIB
            ./memory/env.cpp
            ./memory/footprint.cpp
            ./memory/has_tsx.cpp
            ./memory/makechunk.cpp
            ./memory/futex_mutex.cpp
            ./memory/bassert.cpp
            ./memory/malloc.cpp
            ./memory/cache.cpp
            ./memory/huge_malloc.cpp
            ./memory/large_malloc.cpp
            ./memory/small_malloc.cpp
            ./memory/rng.cpp
            ./memory/stats.cpp
            ./memory/generated_constants.cpp
            )

    add_library(supermalloc SHARED ${SUPER_MALLOC_LIB})

    if (USE_JEMALLOC)
        set(CMAKE_EXE_LINKER_FLAGS "-L${JEMALLOC_LIBDIR} -Wl,-rpath,${JEMALLOC}")
        message("${CMAKE_EXE_LINKER_FLAGS} ${JEMALLOC} ${LTBBMALLOC}")
    endif ()

    set(BASIC_MALLOC
            ./tests/memory/basic/malloc_basic.cpp
            ./tests/memory/basic/malloc_efficiency.cpp)

    if (MEM_COMP_TESTS)
        foreach (sourcefile ${BASIC_MALLOC})
            get_filename_component(exename ${sourcefile} NAME_WE)
            add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile})
            set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
                    PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
                    EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
                    EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
                    OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
                    )
            target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD})
        endforeach (sourcefile ${BASIC_MALLOC})

        foreach (sourcefile ${BASIC_MALLOC})
            set(JSUFIX "jemalloc")
            get_filename_component(exename ${sourcefile} NAME_WE)
            add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${JSUFIX} ${sourcefile})
            set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${JSUFIX}
                    PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
                    EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
                    EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
                    OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}_${JSUFIX}
                    )
            target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${JSUFIX} ${JEMALLOC} ${PTHREAD})
        endforeach (sourcefile ${BASIC_MALLOC})

        foreach (sourcefile ${BASIC_MALLOC})
            set(TSUFIX "tbbmalloc")
            get_filename_component(exename ${sourcefile} NAME_WE)
            add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${TSUFIX} ${sourcefile})
            set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${TSUFIX}
                    PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
                    EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
                    EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
                    OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}_${TSUFIX}
                    )
            target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${TSUFIX} ${LTBBMALLOC} ${PTHREAD})
        endforeach (sourcefile ${BASIC_MALLOC})

        set(CMAKE_CXX_FLAGS "-pthread -mrtm ${CMAKE_CXX_FLAGS}")
        foreach (sourcefile ${BASIC_MALLOC})
            set(SSUFIX "supermalloc")
            set(SUPERMALLOC "-L./ -lsupermalloc -ldl")
            get_filename_component(exename ${sourcefile} NAME_WE)
            add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${SSUFIX} ${sourcefile})
            add_dependencies(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${SSUFIX} supermalloc)
            set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${SSUFIX}
                    PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
                    EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
                    EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
                    OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}_${SSUFIX}
                    )
            target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}_${SSUFIX} ${SUPERMALLOC} ${PTHREAD})
        endforeach (sourcefile ${BASIC_MALLOC})
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

set(LFQ_TESTS_EXES ${LFQ_TESTS})
foreach (sourcefile ${LFQ_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${Boost_LIBRARIES} ${PTHREAD} ${LTBB})
    #    if (NOT "${exename}" MATCHES "db_sanity_test")
    #        add_test(NAME ${exename} COMMAND ${exename}${ARTIFACT_SUFFIX})
    #        add_dependencies(check ${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX})
    #    endif ()
endforeach (sourcefile ${LFQ_TESTS_EXES})

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


set(PERSIST_TESTS_EXES ${PLF_TESTS})
foreach (sourcefile ${PERSIST_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile} ${LF_LIBS})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD} ${PMDK_LIBS})
endforeach (sourcefile ${FOSTER_TESTS_EXES})

set(BWTREE_TESTS_EXES ${BWTREE_TESTS})
foreach (sourcefile ${BWTREE_TESTS_EXES})
    get_filename_component(exename ${sourcefile} NAME_WE)
    add_executable(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${sourcefile} ${BWTREE_LIBS})
    set_target_properties(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX}
            PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_MINRELEASE 1
            EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO 1
            OUTPUT_NAME ${exename}${ARTIFACT_SUFFIX}
            )
    target_link_libraries(${CMAKE_PROJECT_NAME}_${exename}${ARTIFACT_SUFFIX} ${PTHREAD} -latomic)
endforeach (sourcefile ${BWTREE_TESTS_EXES})