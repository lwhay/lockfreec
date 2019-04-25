//
// Created by Michael on 2019-04-25.
//

#include <iostream>
#include "LFResizableHashSet.h"

#define TOTAL_COUNT (1 << 28)

void simpleInsert() {
    OFLFResizableHashSet<uint64_t> set;
    for (int i = 0; i < TOTAL_COUNT; i++) {
        set.add(i);
    }
    for (int i = 0; i < TOTAL_COUNT; i += (TOTAL_COUNT / (1 << 20))) {
        cout << set.contains(i);
    }
}

int main(int argc, char **argv) {
    simpleInsert();
}