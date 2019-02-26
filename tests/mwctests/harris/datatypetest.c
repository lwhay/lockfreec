//
// Created by Michael on 2019-02-26.
//

#include <stdio.h>
#include "stm.h"

int main() {
    r_entry_t *re = (r_entry_t *) malloc(sizeof(r_entry_t));
    w_entry_t *we = (w_entry_t *) malloc(sizeof(w_entry_t));
    printf("%llu %llu %llu %llu %llu %llu\n", sizeof(int), sizeof(unsigned int), sizeof(long), sizeof(unsigned long),
           sizeof(long long), sizeof(unsigned long long));
    printf("%llu %llu %llu %llu\n", sizeof(re), sizeof(we), sizeof(we->addr), sizeof(unsigned long));
    printf("%llu %llu %llu %llu\n", sizeof(*re), sizeof(*we), sizeof(*we->addr), sizeof(uintptr_t));
    free(re);
    free(we);
}