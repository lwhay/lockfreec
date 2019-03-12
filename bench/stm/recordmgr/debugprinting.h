//
// Created by lwh on 19-3-12.
//


#ifndef DEBUGPRINTING_H
#define DEBUGPRINTING_H

#include <atomic>
#include <sstream>

#define COUTATOMIC(coutstr) /*cout<<coutstr*/ \
{ \
    std::stringstream ss; \
    ss<<coutstr; \
    std::cout<<ss.str(); \
}
#define COUTATOMICTID(coutstr) /*cout<<"tid="<<(tid<10?" ":"")<<tid<<": "<<coutstr*/ \
{ \
    std::stringstream ss; \
    ss<<"tid="<<tid<<(tid<10?" ":"")<<": "<<coutstr; \
    std::cout<<ss.str(); \
}

// define USE_TRACE if you want many paths through the code to be traced with std::cout<<"..." statements
#ifdef USE_TRACE
/*PAD;*/
std::atomic_bool ___trace(1);
std::atomic_bool ___validateops(1);
/*PAD;*/
#define TRACE_TOGGLE {bool ___t = ___trace; ___trace = !___t;}
#define TRACE_ON {___trace = true;}
#define TRACE if(___trace)
#else
#define TRACE if(0)
#define TRACE_TOGGLE
#define TRACE_ON
#endif

#endif    /* DEBUGPRINTING_H */
