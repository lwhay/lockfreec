//
// Created by Michael on 2018/12/17.
//

#ifndef LOCKFREEC_REL_PTR_H
#define LOCKFREEC_REL_PTR_H

#include <exception>

#ifndef __MSVC__
typedef unsigned char uchar;
#ifndef __APPLE__
typedef long off_t;
#endif
#ifndef __CYGWIN__
typedef unsigned long long uint64_t;
#endif
#endif

#if defined(__MINGW64__) || defined(__CYGWIN__) || defined(linux) || defined(__APPLE__)
#define EXCHANGE                        __sync_fetch_and_add
#define CAS                             __sync_val_compare_and_swap
#else
#define EXCHANGE                        InterlockedExchange
#define CAS                             InterlockedCompareExchange
#endif

template<typename T>
class rel_ptr {
    uchar *base_address;
    uint64_t offset;
public:
    rel_ptr() : offset(0) {}

    rel_ptr(const T *abs_ptr) : offset((uchar *) abs_ptr - base_address) {}

    rel_ptr(uint64_t rel_addr) : offset(rel_addr) {}

    template<typename U>
    rel_ptr(rel_ptr<U> rptr) : offset((uchar *) rptr.abs() - base_address) {}

    T &operator*() {
        if (!offset) {
            throw std::exception();
        }
        return *(T *) (base_address + offset);
    }

    T *operator->() {
        if (!offset) {
            throw std::exception();
        }
        return (T *) (base_address + offset);
    }
};

#endif //LOCKFREEC_REL_PTR_H
