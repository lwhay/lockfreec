//
// Created by Michael on 2018/12/11.
//

#include "lf_binarytree.h"
#include "internal.h"

int main() {
    int x = -1;
    int y = 0;
    int z = 1;
    printf("%d %d %d\n", lf_binarytree_int_comparator((void *) &x, (void *) &y),
           lf_binarytree_int_comparator((void *) &x, (void *) &x),
           lf_binarytree_int_comparator((void *) &z, (void *) &y));
}