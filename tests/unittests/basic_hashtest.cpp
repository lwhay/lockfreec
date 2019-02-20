//
// Created by Michael on 2019/2/14.
//
#include <unordered_set>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "basic_hashfunc.h"

//char *rdfFilePath = "d:/trie_data/dbpedia_all.nt";
char *rdfFilePath = "d:/trie_data/University_rand_longstr.nt";
#define TOTAL_NUMBER (1 << 25)
#define BUCKET_NUMBER (1 << 16)

void testInt64() {
    int counters[BUCKET_NUMBER];
    memset(counters, 0, BUCKET_NUMBER * sizeof(int));
    struct timeval tick;
    gettimeofday(&tick, NULL);
    srand(tick.tv_usec);
    int rint = rand();
    printf("%d\n", rint);
    struct timeval begin;
    gettimeofday(&begin, NULL);
    for (int i = 0; i < TOTAL_NUMBER; i++) {
        uint64_t val = ((uint64_t) rand() << 32) + ((uint64_t) rand());
        uint32_t idx = __hash_func(&val, 8) % BUCKET_NUMBER;
        counters[idx]++;
    }
    struct timeval end;
    gettimeofday(&end, NULL);
    uint64_t total = 0;
    for (int i = 0; i < BUCKET_NUMBER; i++) {
        total += counters[i];
        printf("%d\t", counters[i]);
        if ((i + 1) % 32 == 0) {
            printf("\n");
        }
    }
    printf("%llu, %d\n", total, ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec));
}

void testRDFString() {
    char *line = NULL;
    const char *delim = " ";
    size_t len;
    uint64_t lc = 0;
    int counters[BUCKET_NUMBER];
    memset(counters, 0, BUCKET_NUMBER * sizeof(int));
    FILE *fp = fopen(rdfFilePath, "rb+");
    struct timeval begin;
    gettimeofday(&begin, NULL);
    while (getline(&line, &len, fp) != -1) {
        int cnt = 0;
        char *p = line;
        do {
            if (cnt++ == 3) {
                break;
            }
            char *pn = strpbrk(p, delim);
            if (!pn) {
                break;
            } else {
                uint32_t idx = __hash_func(p, pn - p) % BUCKET_NUMBER;
                if (lc % 1000000 == 0 && cnt == 1) {
                    printf("\t%s\n", p);
                }
                counters[idx]++;
            }
            p = pn + 1;
        } while (p);
        lc++;
    }
    struct timeval end;
    gettimeofday(&end, NULL);
    fclose(fp);
    uint64_t total = 0;
    for (int i = 0; i < BUCKET_NUMBER; i++) {
        total += counters[i];
        printf("%6d\t", counters[i]);
        if ((i + 1) % 32 == 0) {
            printf("\n");
        }
    }
    printf("%llu, %llu, %d\n", lc, total, ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec));
}

void testUniqRDFString() {
    std::unordered_set<std::string> uniq;
    char tmpstr[4096];
    char *line = NULL;
    const char *delim = " ";
    size_t len;
    uint64_t lc = 0;
    int counters[BUCKET_NUMBER];
    memset(counters, 0, BUCKET_NUMBER * sizeof(int));
    FILE *fp = fopen(rdfFilePath, "rb+");
    struct timeval begin;
    gettimeofday(&begin, NULL);
    while (getline(&line, &len, fp) != -1) {
        int cnt = 0;
        char *p = line;
        do {
            if (cnt++ == 3) {
                break;
            }
            char *pn = strpbrk(p, delim);
            if (!pn) {
                break;
            } else {
                memset(tmpstr, 0, 4096);
                memcpy(tmpstr, p, pn - p);
                bool found = true;
                if (uniq.find(tmpstr) == uniq.end()) {
                    found = false;
                    uniq.insert(tmpstr);
                    uint32_t idx = __hash_func(p, pn - p) % BUCKET_NUMBER;
                    counters[idx]++;
                }
                if (lc % 1000000 == 0 && cnt == 1) {
                    printf("\t%d: %s\n", found, p);
                }
            }
            p = pn + 1;
        } while (p);
        lc++;
    }
    struct timeval end;
    gettimeofday(&end, NULL);
    fclose(fp);
    uint64_t total = 0;
    for (int i = 0; i < BUCKET_NUMBER; i++) {
        total += counters[i];
        printf("%6d\t", counters[i]);
        if ((i + 1) % 32 == 0) {
            printf("\n");
        }
    }
    printf("%llu, %llu, %d\n", lc, total, ((end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec));
}

int main() {
    testInt64();
    printf("******************************************************************\n");
    testRDFString();
    printf("******************************************************************\n");
    testUniqRDFString();
}