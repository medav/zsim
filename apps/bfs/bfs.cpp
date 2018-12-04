#include <iostream>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <stdint.h>

#include "zsim_hooks.h"

#include <pthread.h>

typedef struct _TreeNode {
    struct _TreeNode * left;
    struct _TreeNode * right;
    uint64_t data;
} TreeNode;

std::mutex m;
std::queue<TreeNode *> q;

void worker(int id) {
    TreeNode * tn;
    while (1) {
        m.lock();
        if (q.empty()) {
            m.unlock();
            return;
        }
        tn = q.front();
        q.pop();
        if (tn->left) q.push(tn->left);
        if (tn->right) q.push(tn->right);
        m.unlock();

        tn->data *= 100;
    }
}

void worker_entry(int id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(id, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    worker(id);
}

TreeNode * GenTree(int depth) {
    if (depth == 0) return NULL;
    TreeNode * tree = (TreeNode *)malloc(sizeof(TreeNode));

    tree->left = GenTree(depth - 1);
    tree->right = GenTree(depth - 1);
    tree->data = (uint64_t)tree->data;

    return tree;
}

int main(int argc, char * argv[]) {
    int num_cores = atoi(argv[1]);
    int tree_depth = atoi(argv[2]);

    std::vector<std::thread> threads;

    TreeNode * tree = GenTree(tree_depth);

    printf("Setting affinity for thread 0\n");
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    q.push(tree);

    printf("Spawning worker threads\n");
    zsim_roi_begin();
    for (int id = 1; id < num_cores; id++) {
        threads.push_back(std::thread(worker, id));
    }

    zsim_heartbeat();

    printf("Working...\n");
    worker(0);

    printf("Waiting for threads...\n");
    for (int id = 0; id < num_cores - 1; id++) {
        threads[id].join();
    }

    zsim_roi_end();

    return 0;
}
