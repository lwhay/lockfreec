#include "lf_basic.h"

/*static LF_INLINE */atom_t abstraction_cas(volatile atom_t* destination, atom_t exchange, atom_t compare) {
    atom_t  rv;
    assert(destination != NULL);
    LF_BARRIER_COMPILER_FULL;
    rv = (atom_t) __sync_val_compare_and_swap(destination, compare, exchange);
    LF_BARRIER_COMPILER_FULL;
    return (rv);
}

/*static LF_INLINE */unsigned char abstraction_dcas(volatile atom_t* destination, atom_t* exchange, atom_t* compare) {
    unsigned char cas_result;
    assert(destination != NULL);
    assert(exchange != NULL);
    assert(compare != NULL);
    __asm__ __volatile__
            (
                "lock;"
                "cmpxchg16b    %0;"
                "setz           %3"
                : "+m" (*(volatile atom_t (*)[2]) destination), "+a" (*compare), "+d" (*(compare+1)), "=q" (cas_result)
                : "b" (*exchange), "c" (*(exchange+1))
                : "cc", "memory"
                );
    return (cas_result);
}

/*static LF_INLINE */atom_t abstraction_increment(volatile atom_t* value) {
    atom_t rv;
    assert(value != NULL);
    LF_BARRIER_COMPILER_FULL;
    rv = (atom_t)__sync_add_and_fetch(value, 1);
    LF_BARRIER_COMPILER_FULL;
    return (rv);
}

/*static LF_INLINE */atom_t abstraction_sub(volatile atom_t* value) {
    atom_t rv;
    assert(value != NULL);
    LF_BARRIER_COMPILER_FULL;
    rv = (atom_t)__sync_sub_and_fetch(value, 1);
    LF_BARRIER_COMPILER_FULL;
    return (rv);
}

void* abstraction_malloc(size_t size) {
    return (malloc(size));
}

void abstraction_free(void *memory) {
    free(memory);
    return;
}

void aligned_free(void *memory) {
    assert(memory != NULL);
    abstraction_free(*((void**)memory - 1));
    return;
}

void* aligned_malloc(size_t size, size_t align_in_bytes) {
    void *original_memory, *memory;
    size_t offset;
    original_memory = memory = abstraction_malloc(size + sizeof(void*) + align_in_bytes);
    if (NULL != memory) {
        memory = (void**)memory + 1;
        offset = align_in_bytes - (size_t)memory % align_in_bytes;
        memory = (unsigned char*)memory + offset;
        *((void**)memory - 1) = original_memory;
    }
    return (memory);
}
