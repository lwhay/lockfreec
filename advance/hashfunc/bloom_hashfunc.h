//
// Created by Michael on 2019/2/15.
//

#ifndef LOCKFREEC_BLOOM_HASHFUNC_H
#define LOCKFREEC_BLOOM_HASHFUNC_H

#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#define MIN_ATOMIC_GCC_VERSION 7

#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)

#include <stdatomic.h>

#define INT_BIT (sizeof(_Atomic(int)) * CHAR_BIT)
#else
#define INT_BIT (sizeof(int) * CHAR_BIT)
#endif


#ifndef NDEBUG
#define BV_BOUND_CHECK(size, idx) assert(idx < size)
#else
#define BV_BOUND_CHECK(size, idx)
#endif

typedef struct BitVector {
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
    _Atomic int *field;
#else
    int *field;
#endif
    size_t size;
} bitvector_t;

static int bv_init(bitvector_t *bv, size_t k) {
    bv->size = k;
    size_t ints = (k + (INT_BIT - 1)) / INT_BIT;
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
    bv->field = calloc(ints, sizeof(_Atomic (int)));
#else
    bv->field = calloc(ints, sizeof(int));
#endif
    if (bv->field == NULL) {
        return -1;
    }
    return 0;
}

static bitvector_t *bv_create(size_t k) {
    bitvector_t *v = malloc(sizeof(bitvector_t));
    if (v == NULL || bv_init(v, k)) {
        free(v);
        return NULL;
    }
    return v;
}

static void bv_free(bitvector_t *bv) {
    free(bv->field);
    bv->size = 0;
}

static void bv_destroy(bitvector_t *bv) {
    bv_free(bv);
    free(bv);
}

static void bv_set(bitvector_t *bv, size_t k) {
    BV_BOUND_CHECK(bv->size, k);
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
    atomic_fetch_or(&(bv->field[k / INT_BIT]), 1 << (k % INT_BIT));
#else
    __sync_fetch_and_or(&(bv->field[k / INT_BIT]), 1 << (k % INT_BIT));
#endif
}

static void bv_unset(bitvector_t *bv, size_t k) {
    BV_BOUND_CHECK(bv->size, k);
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
    atomic_fetch_and(&(bv->field[k / INT_BIT]), ~(1 << (k % INT_BIT)));
#else
    __sync_fetch_and_and(&(bv->field[k / INT_BIT]), ~(1 << (k % INT_BIT)));
#endif
}

static void bv_toggle(bitvector_t *bv, size_t k) {
    BV_BOUND_CHECK(bv->size, k);
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
    atomic_fetch_xor(&(bv->field[k / INT_BIT]), 1 << (k % INT_BIT));
#else
    __sync_fetch_and_xor(&(bv->field[k / INT_BIT]), 1 << (k % INT_BIT));
#endif
}

static int bv_get(bitvector_t *bv, size_t k) {
    BV_BOUND_CHECK(bv->size, k);
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
    return ((atomic_load(&(bv->field[k / INT_BIT])) & (1 << (k % INT_BIT))) != 0);
#else
    __sync_synchronize();
    return (((bv->field[k / INT_BIT] & (1 << (k % INT_BIT)))) != 0);
#endif
}

// murmur3 hash

#ifdef __GNUC__
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

static FORCE_INLINE uint32_t rotl32(uint32_t x, int8_t r) {
    return (x << r) | (x >> (32 - r));
}

static FORCE_INLINE uint64_t rotl64(uint64_t x, int8_t r) {
    return (x << r) | (x >> (64 - r));
}

#define ROTL32(x, y)    rotl32(x,y)
#define ROTL64(x, y)    rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

#define getblock(p, i) (p[i])

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

static FORCE_INLINE uint32_t fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

static FORCE_INLINE uint64_t fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;

    return k;
}

void MurmurHash3_x86_32(const void *key, int len, uint32_t seed, void *out) {
    const uint8_t *data = (const uint8_t *) key;
    const int nblocks = len / 4;
    int i;

    uint32_t h1 = seed;

    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    const uint32_t *blocks = (const uint32_t *) (data + nblocks * 4);

    for (i = -nblocks; i; i++) {
        uint32_t k1 = getblock(blocks, i);

        k1 *= c1;
        k1 = ROTL32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = ROTL32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t *tail = (const uint8_t *) (data + nblocks * 4);

    uint32_t k1 = 0;

    switch (len & 3) {
        case 3:
            k1 ^= tail[2] << 16;
        case 2:
            k1 ^= tail[1] << 8;
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = ROTL32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
    };

    h1 ^= len;

    h1 = fmix32(h1);

    *(uint32_t *) out = h1;
}

void MurmurHash3_x86_128(const void *key, const int len, uint32_t seed, void *out) {
    const uint8_t *data = (const uint8_t *) key;
    const int nblocks = len / 16;
    int i;

    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;

    uint32_t c1 = 0x239b961b;
    uint32_t c2 = 0xab0e9789;
    uint32_t c3 = 0x38b34ae5;
    uint32_t c4 = 0xa1e38b93;

    //----------
    // body

    const uint32_t *blocks = (const uint32_t *) (data + nblocks * 16);

    for (i = -nblocks; i; i++) {
        uint32_t k1 = getblock(blocks, i * 4 + 0);
        uint32_t k2 = getblock(blocks, i * 4 + 1);
        uint32_t k3 = getblock(blocks, i * 4 + 2);
        uint32_t k4 = getblock(blocks, i * 4 + 3);

        k1 *= c1;
        k1 = ROTL32(k1, 15);
        k1 *= c2;
        h1 ^= k1;

        h1 = ROTL32(h1, 19);
        h1 += h2;
        h1 = h1 * 5 + 0x561ccd1b;

        k2 *= c2;
        k2 = ROTL32(k2, 16);
        k2 *= c3;
        h2 ^= k2;

        h2 = ROTL32(h2, 17);
        h2 += h3;
        h2 = h2 * 5 + 0x0bcaa747;

        k3 *= c3;
        k3 = ROTL32(k3, 17);
        k3 *= c4;
        h3 ^= k3;

        h3 = ROTL32(h3, 15);
        h3 += h4;
        h3 = h3 * 5 + 0x96cd1c35;

        k4 *= c4;
        k4 = ROTL32(k4, 18);
        k4 *= c1;
        h4 ^= k4;

        h4 = ROTL32(h4, 13);
        h4 += h1;
        h4 = h4 * 5 + 0x32ac3b17;
    }

    const uint8_t *tail = (const uint8_t *) (data + nblocks * 16);

    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;

    switch (len & 15) {
        case 15:
            k4 ^= tail[14] << 16;
        case 14:
            k4 ^= tail[13] << 8;
        case 13:
            k4 ^= tail[12] << 0;
            k4 *= c4;
            k4 = ROTL32(k4, 18);
            k4 *= c1;
            h4 ^= k4;

        case 12:
            k3 ^= tail[11] << 24;
        case 11:
            k3 ^= tail[10] << 16;
        case 10:
            k3 ^= tail[9] << 8;
        case 9:
            k3 ^= tail[8] << 0;
            k3 *= c3;
            k3 = ROTL32(k3, 17);
            k3 *= c4;
            h3 ^= k3;

        case 8:
            k2 ^= tail[7] << 24;
        case 7:
            k2 ^= tail[6] << 16;
        case 6:
            k2 ^= tail[5] << 8;
        case 5:
            k2 ^= tail[4] << 0;
            k2 *= c2;
            k2 = ROTL32(k2, 16);
            k2 *= c3;
            h2 ^= k2;

        case 4:
            k1 ^= tail[3] << 24;
        case 3:
            k1 ^= tail[2] << 16;
        case 2:
            k1 ^= tail[1] << 8;
        case 1:
            k1 ^= tail[0] << 0;
            k1 *= c1;
            k1 = ROTL32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
    };

    h1 ^= len;
    h2 ^= len;
    h3 ^= len;
    h4 ^= len;

    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;

    h1 = fmix32(h1);
    h2 = fmix32(h2);
    h3 = fmix32(h3);
    h4 = fmix32(h4);

    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;

    ((uint32_t *) out)[0] = h1;
    ((uint32_t *) out)[1] = h2;
    ((uint32_t *) out)[2] = h3;
    ((uint32_t *) out)[3] = h4;
}

void MurmurHash3_x64_128(const void *key, const int len, const uint32_t seed, void *out) {
    const uint8_t *data = (const uint8_t *) key;
    const int nblocks = len / 16;
    int i;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
    uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

    const uint64_t *blocks = (const uint64_t *) (data);

    for (i = 0; i < nblocks; i++) {
        uint64_t k1 = getblock(blocks, i * 2 + 0);
        uint64_t k2 = getblock(blocks, i * 2 + 1);

        k1 *= c1;
        k1 = ROTL64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = ROTL64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = ROTL64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = ROTL64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    const uint8_t *tail = (const uint8_t *) (data + nblocks * 16);

    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch (len & 15) {
        case 15:
            k2 ^= (uint64_t) (tail[14]) << 48;
        case 14:
            k2 ^= (uint64_t) (tail[13]) << 40;
        case 13:
            k2 ^= (uint64_t) (tail[12]) << 32;
        case 12:
            k2 ^= (uint64_t) (tail[11]) << 24;
        case 11:
            k2 ^= (uint64_t) (tail[10]) << 16;
        case 10:
            k2 ^= (uint64_t) (tail[9]) << 8;
        case 9:
            k2 ^= (uint64_t) (tail[8]) << 0;
            k2 *= c2;
            k2 = ROTL64(k2, 33);
            k2 *= c1;
            h2 ^= k2;

        case 8:
            k1 ^= (uint64_t) (tail[7]) << 56;
        case 7:
            k1 ^= (uint64_t) (tail[6]) << 48;
        case 6:
            k1 ^= (uint64_t) (tail[5]) << 40;
        case 5:
            k1 ^= (uint64_t) (tail[4]) << 32;
        case 4:
            k1 ^= (uint64_t) (tail[3]) << 24;
        case 3:
            k1 ^= (uint64_t) (tail[2]) << 16;
        case 2:
            k1 ^= (uint64_t) (tail[1]) << 8;
        case 1:
            k1 ^= (uint64_t) (tail[0]) << 0;
            k1 *= c1;
            k1 = ROTL64(k1, 31);
            k1 *= c2;
            h1 ^= k1;
    };

    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    ((uint64_t *) out)[0] = h1;
    ((uint64_t *) out)[1] = h2;
}

#define SALT_CONSTANT 0x97c29b3a

#ifdef __GNUC__
#define unlikely(C) __builtin_expect((C), 0)
#else
#define unlikely(C) (C)
#endif

#if __x86_64__
#define hashfunc MurmurHash3_x64_128
#elif __i686__
#define hashfunc MurmurHash3_x86_128
#else
#error "Not a currently supported architecture."
#endif

typedef struct {
    size_t capacity;
    size_t nhashes;
    double error_rate;
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
    _Atomic size_t nitems;
#else
    size_t nitems;
#endif
    size_t size;
    bitvector_t *bitvector;
} bloom_t;

static size_t bloom_optimal_hash_count(const size_t bits, const size_t items) {
    size_t k = (size_t) ceil(((double) bits * log(2)) / items);
    if (k < 1)
        return 1;
    return k;
}

static size_t bloom_bits_for_accuracy(size_t capacity, double error_rate) {
    return (size_t) ceil((-log(error_rate) * capacity) / (log(2) * log(2)));
}

static void gen_hashes(const char *key, size_t key_len, size_t *hashes, size_t nhashes, size_t max_size) {
    uint32_t hashvalues[4];
    hashfunc(key, key_len, SALT_CONSTANT, hashvalues);
    uint32_t a = hashvalues[0];
    uint32_t b = hashvalues[1];

    hashes[0] = a % max_size;
    size_t i;
    for (i = 1; i < nhashes; ++i) {
        hashes[i] = (hashes[i - 1] + b) % max_size;
    }
}

int bloom_init(bloom_t *bloom, size_t capacity, double error_rate) {
    bloom->capacity = capacity;
    bloom->nitems = 0;
    bloom->error_rate = error_rate;
    bloom->size = bloom_bits_for_accuracy(capacity, error_rate);
    bloom->nhashes = bloom_optimal_hash_count(bloom->size, capacity);
    bloom->bitvector = bv_create(bloom->size);
    if (bloom->bitvector == NULL) {
        return -1;
    }
    return 0;
}

bloom_t *bloom_create(size_t capacity, double error_rate) {
    bloom_t *bloom = malloc(sizeof(bloom_t));
    if (bloom == NULL || bloom_init(bloom, capacity, error_rate)) {
        free(bloom);
        return NULL;
    }
    return bloom;
}

int bloom_attach(bloom_t *bloom, const char *key, size_t key_len, size_t *hashes) {
    if (hashes == NULL)
        return -1;
    gen_hashes(key, key_len, hashes, bloom->nhashes, bloom->size);
    int seen = 1;        /* andable values for seeing if the value was already set */
    size_t i;
    for (i = 0; i < bloom->nhashes; ++i) {
        int cval = bv_get(bloom->bitvector, hashes[i]);
        seen &= cval;
        if (!cval)
            bv_set(bloom->bitvector, hashes[i]);
    }
    if (!seen)
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
        atomic_fetch_add_explicit(&(bloom->nitems), 1, memory_order_relaxed);
#else
        __sync_fetch_and_add(&(bloom->nitems), 1);
#endif
    return seen;
}

int bloom_add(bloom_t *bloom, const char *key, size_t key_len) {
    size_t *hashes = calloc(bloom->nhashes, sizeof(size_t));
    if (hashes == NULL)
        return -1;
    gen_hashes(key, key_len, hashes, bloom->nhashes, bloom->size);
    int seen = 1;        /* andable values for seeing if the value was already set */
    size_t i;
    for (i = 0; i < bloom->nhashes; ++i) {
        int cval = bv_get(bloom->bitvector, hashes[i]);
        seen &= cval;
        if (!cval)
            bv_set(bloom->bitvector, hashes[i]);
    }
    free(hashes);
    if (!seen)
#if (__GNUC__ > MIN_ATOMIC_GCC_VERSION)
        atomic_fetch_add_explicit(&(bloom->nitems), 1, memory_order_relaxed);
#else
        __sync_fetch_and_add(&(bloom->nitems), 1);
#endif
    return seen;
}

int bloom_obtain(bloom_t *bloom, const char *key, size_t key_len, size_t *hashes) {
    gen_hashes(key, key_len, hashes, bloom->nhashes, bloom->size);
    size_t i;
    for (i = 0; i < bloom->nhashes; ++i) {
        if (!bv_get(bloom->bitvector, hashes[i])) {
            return 0;
        }
    }
    return 1;
}

int bloom_get(bloom_t *bloom, const char *key, size_t key_len) {
    size_t *hashes = calloc(bloom->nhashes, sizeof(size_t));
    gen_hashes(key, key_len, hashes, bloom->nhashes, bloom->size);
    size_t i;
    for (i = 0; i < bloom->nhashes; ++i) {
        if (!bv_get(bloom->bitvector, hashes[i])) {
            free(hashes);
            return 0;
        }
    }
    free(hashes);
    return 1;
}

void bloom_destroy(bloom_t *bloom) {
    bv_destroy(bloom->bitvector);
    free(bloom);
}

#undef BV_BOUND_CHECK
#undef INT_BIT
#endif //LOCKFREEC_BLOOM_HASHFUNC_H
