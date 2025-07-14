#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <atomic>
#include <sstream>
#include <chrono>

#include "../harris_ll/harris.h"

#define SOFTWARE_BARRIER asm volatile("": : :"memory")

#define COUTATOMIC(coutstr) /*cout<<coutstr*/ \
{ \
    std::stringstream ss; \
    ss<<coutstr; \
    std::cout<<ss.str(); \
}

intset_t* set;
int zone = 0;
int THREADS = 2;
int track_size[2];
int track_keysum[2];
std::atomic_int running;
bool start;

void prefill(int idx) {
    for (int i = 0; i < 30; i++) {
        int key = rand() % 100 + 1;
        int val = rand() % 1000;
        if (harris_insert(set, idx, zone, key, val)) {
            track_size[idx]++;
            track_keysum[idx] += key;
        } else {
            i--; // try again
        }
    }
}

void *perform_trial(void *arg) {
    int idx = *((int*) arg);
    srand(time(NULL));

    prefill(idx);

    running.fetch_add(1);
    __sync_synchronize();
    while (!start) { __sync_synchronize(); }

    for (int i = 0; i < 10; i++) {
        k_t del_key;
        int del_idx, del_zone;
        val__t retval = harris_delete_min(set, &del_key, &del_idx, &del_zone);
        COUTATOMIC("removed: " << del_key << "\n");
        track_size[idx]--;
        track_keysum[idx] -= del_key;
    }

    running.fetch_add(-1);
    while (running.load()) { /* wait */ }
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    srand(time(NULL));
    running = 0;
    start = false;

    set = set_new();

    pthread_t *threads[THREADS];
    int ids[THREADS];
    for (int i = 0; i < THREADS; ++i) {
        threads[i] = new pthread_t;
        ids[i] = i;
        track_size[i] = 0;
        track_keysum[i] = 0;
    }

    for (int i = 0; i < THREADS; ++i) {
        if (pthread_create(threads[i], NULL, perform_trial, &ids[i])) {
            std::cerr<<"ERROR: could not create thread"<<std::endl;
            exit(-1);
        }
    }

    while (running.load() < THREADS) {}

    print_set(set);

    __sync_synchronize();
    start = true;
    SOFTWARE_BARRIER;

    timespec tsNap;
    tsNap.tv_sec = 0;
    tsNap.tv_nsec = 1000000; // 10mss
    while (running.load() > 0) {
        nanosleep(&tsNap, NULL);
    }

    __sync_synchronize();
    // join all threads
    for (int i = 0; i < THREADS; ++i) {
        COUTATOMIC("joining thread "<<i<<std::endl);
        if (pthread_join(*(threads[i]), NULL)) {
            std::cerr<<"ERROR: could not join thread"<<std::endl;
            exit(-1);
        }
    }
    __sync_synchronize();

    

    // // remove the 5 minimum
    // for (int i = 0; i < 1; i++) {
    //     int del_idx, del_zone;
    //     k_t del_key;
    //     int ret = harris_delete_idx(set, 0, zone, &del_key);
    //     track_size--;
    //     track_keysum -= del_key;
    //     std::cout << "removed key " << del_key << "\n";
    // }

    print_set(set);

    int tot_keysum = 0;
    int tot_size = 0;
    for (int i = 0; i < THREADS; i++) {
        tot_keysum += track_keysum[i];
        tot_size += track_size[i];
    }

    std::cout << "\n\nTHREAD INFO\n\tsize: " << tot_size << "\n\tkeysum: " << tot_keysum << "\n";

    int size = set_size(set);
    int keysum = set_keysum(set);

    std::cout << "DS INFO\n\tsize: " << size << "\n\tkeysum: " << keysum << "\n";
    set_delete(set);
}