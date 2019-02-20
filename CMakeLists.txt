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
include_directories(basic)
include_directories(cases)
include_directories(mwcas)

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

option(WITH_LF_TESTS "build with lftests" ON)
if (WITH_LF_TESTS)
    set(LF_TEST
            tests/it_lockfree_all.c
            tests/it_dcastest.c
            tests/it_binarytree.c)
endif ()

option(WITH_MWLF_TESTS "build with multiword lftests" ON)
if (WITH_MWLF_TESTS)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(MWLF_TEST
                tests/it_moodycqtest.cpp
                tests/it_basiclfqueuetest.cpp)
    else ()
        set(MWLF_TEST
                tests/it_mwcastest.cpp
                tests/mwctests/utebrtest.cpp
                tests/it_moodycqtest.cpp
                tests/it_basiclfqueuetest.cpp)
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
