//
// Created by Michael on 2018/12/9.
//
#include <stdlib.h>
#include <stdio.h>
#include "btrees.h"

#define DATAPATH "./kv.data"

#define SHUFFLE 1

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
    char buffer[PAGE_SIZE];
    int count = PAGE_SIZE / (2 * sizeof(int));
    int round = NUM / count;
    FILE *dp = fopen(DATAPATH, "rb+");
    if (dp == NULL) {
        dp = fopen(DATAPATH, "wb+");
        int tick = 0;
#if SHUFFLE
        int *input = (int *) malloc(NUM * sizeof(int));
        for (int i = 0; i < NUM; i++) {
            input[i] = i;
        }
        for (int i = 0; i < NUM; i++) {
            unsigned long long offset =
                    ((unsigned long long) rand() * rand() * rand() + (unsigned long long) rand() * rand() + rand()) %
                    NUM;
            int idx = (int) offset;
            int tmp = input[i];
            input[i] = input[idx];
            input[idx] = tmp;
        }
        for (int i = 0; i < NUM; i++) {
            fwrite(&input[i], sizeof(int), 1, dp);
            fwrite(&input[i], sizeof(int), 1, dp);
        }
        free(input);
#else
        for (int i = 0; i < round; i++) {
            for (int j = 0; j < count; j += 2) {
                if (tick != i * count / 2 + j / 2) {
                    printf("Value: %d exepected: %d\n", i * count + j / 2, tick);
                }
                tick++;
                ((int *) buffer)[j] = i * count + j / 2;
                ((int *) buffer)[j + 1] = j;
            }
            fwrite(buffer, PAGE_SIZE, 1, dp);
        }
#endif
        fclose(dp);
        dp = fopen(DATAPATH, "rb+");
    }
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