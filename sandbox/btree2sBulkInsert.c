//
// Created by Michael on 2018/12/9.
//

//#include <stdlib.h>
#include <stdio.h>
#include "btrees.h"

#define DATAPATH "./kv.data"

#define NUM (1 << 30)

#define PAGE_SIZE (1 << 16)

int main(int argc, char **argv) {
    uint bits = 12;
    uint map = 8192;
    uint pgblk = 16;
    time_t tod[1];
    time(tod);
    BtDb *bt = bt_open("./int.idx", BT_rw, bits, map, pgblk);
    char key[256];
    int line = 0;
    FILE *dp = fopen(DATAPATH, "rb+");
    char buffer[PAGE_SIZE];
    int count = PAGE_SIZE / (2 * sizeof(int));
    int round = NUM / count;
    struct timeval tpstart, tpend;
    gettimeofday(&tpstart, NULL);
    for (int i = 0; i < round; i++) {
        fread(buffer, PAGE_SIZE, 1, dp);
        for (int j = 0; j < count; j++) {
            memset(key, 0, 256);
            sprintf(key, "%d", ((int *) buffer)[2 * j]);
            bt_insertkey(bt, key, strlen(key), 0, line++, *tod);
            if (line % (NUM / 100) == 0) {
                gettimeofday(&tpend, NULL);
                double timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
                printf("\nTime: %lf\n", timeused);
                printf(".");
            }
        }
    }
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    printf("\nTime: %lf\n", timeused);
}