#include <iostream>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <vector>
#include <condition_variable>

#include <pthread.h>

#define NUMTILES 64
#define MATSIZE 20

typedef struct _global_data {
    int mat_a[MATSIZE][MATSIZE];
    int mat_b[MATSIZE][MATSIZE];
    int output[MATSIZE][MATSIZE];
} global_data;

static inline void compute_element(
    int mat_a[MATSIZE][MATSIZE],
    int mat_b[MATSIZE][MATSIZE],
    int output[MATSIZE][MATSIZE],
    int row,
    int col)
{
    for (int i = 0; i < MATSIZE; i++) {
        output[row][col] += mat_a[row][i] * mat_b[i][col];
    }
}


void worker(int id, global_data * gd) {
    int row;
    int col;

    for (id; id < MATSIZE * MATSIZE; id += NUMTILES) {
        row = id / MATSIZE;
        col = id % MATSIZE;

        compute_element(gd->mat_a, gd->mat_b, gd->output, row, col);
    }
}

void worker_entry(int id, global_data * gd) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(id, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    worker(id, gd);
}

void read_csv(int dest[MATSIZE][MATSIZE], const char * filename) {
    int row;
    int col;
    FILE * f = fopen(filename, "r");

    for (int i = 0; i < MATSIZE * MATSIZE; i++) {
        row = i / MATSIZE;
        col = i % MATSIZE;

        fscanf(f, "%d ", &dest[row][col]);
    }

    fclose(f);
}

int main() {
    global_data * gd = new global_data;
    std::vector<std::thread> threads;

    printf("Reading matrices\n");
    read_csv(gd->mat_a, "apps/matmul/mat_a.csv");
    read_csv(gd->mat_b, "apps/matmul/mat_b.csv");

    memset(gd->output, 0x00, MATSIZE * MATSIZE * sizeof(int));

    printf("Setting affinity for thread 0\n");
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    printf("Spawning worker threads\n");
    for (int id = 1; id < NUMTILES; id++) {
        threads.push_back(std::thread(worker, id, gd));
    }

    printf("Working...\n");
    worker(0, gd);

    printf("Waiting for threads...\n");
    for (int id = 0; id < NUMTILES - 1; id++) {
        threads[id].join();
    }

    for (int row = 0; row < MATSIZE; row++) {
        for (int col = 0; col < MATSIZE; col++) {
            printf("%d, ", gd->output[row][col]);
        }
        printf("\n");
    }

    delete gd;

    return 0;
}
