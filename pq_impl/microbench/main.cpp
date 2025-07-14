#define MICROBENCH

#ifdef __CYGWIN__
    typedef long long __syscall_slong_t;
#endif

typedef long long test_type;

#include <limits>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <atomic>
#include <chrono>
#include <cassert>
#include <unistd.h>   
#include <set>
#include "globals.h"
#include "globals_extern.h"
#include "../common/random.h"
#include "../common/binding.h"
#include "../common/papi_util_impl.h"
#ifdef USE_DEBUGCOUNTERS
    #include "debugcounters.h"
#endif
#include "data_structures.h"

#ifdef ARRAY_SKIPLIST
OPTSTM2_GLOBALS_INITIALIZER;
#endif

using namespace std;

#if !defined USE_DEBUGCOUNTERS && !defined USE_GSTATS
#error "Must define either USE_GSTATS or USE_DEBUGCOUNTERS."
#endif

struct main_globals_t {
    volatile char padding0[PREFETCH_SIZE_BYTES];
    Random rngs[MAX_TID_POW2*PREFETCH_SIZE_WORDS]; // create per-thread random number generators (padded to avoid false sharing)
    volatile char padding1[PREFETCH_SIZE_BYTES];

    // variables used in the concurrent test
    volatile char padding2[PREFETCH_SIZE_BYTES];
    chrono::time_point<chrono::high_resolution_clock> startTime;
    chrono::time_point<chrono::high_resolution_clock> endTime;
    volatile char padding3[PREFETCH_SIZE_BYTES];
    long elapsedMillis;
    long elapsedMillisNapping;
    volatile char padding4[PREFETCH_SIZE_BYTES];

    bool start;
    bool done;
    volatile char padding5[PREFETCH_SIZE_BYTES];
    atomic_int running; // number of threads that are running
    volatile char padding6[PREFETCH_SIZE_BYTES];
    atomic_int total_running;
    volatile char padding13[PREFETCH_SIZE_BYTES];
#ifdef USE_DEBUGCOUNTERS
    debugCounter * keysum; // key sum hashes for all threads (including for prefilling)
    debugCounter * prefillSize;
#endif
    volatile char padding7[PREFETCH_SIZE_BYTES];

    void *__ds; // the data structure

    volatile char padding8[PREFETCH_SIZE_BYTES];
    test_type __garbage;

    volatile char padding9[PREFETCH_SIZE_BYTES];
    volatile long long prefillIntervalElapsedMillis;
    volatile char padding10[PREFETCH_SIZE_BYTES];
    long long prefillKeySum;
    volatile char padding11[PREFETCH_SIZE_BYTES];int num_phases;
    int phased_workload[10][2]; // supports a max of 10 workloads
    chrono::time_point<chrono::high_resolution_clock> timed_per_phased[10];
    volatile char padding12[PREFETCH_SIZE_BYTES];
    unsigned int seeds_[96];
};

main_globals_t glob = {0,};
pthread_barrier_t WaitForAll;

const long long PREFILL_INTERVAL_MILLIS = 100;

long size_chunkedpq = 0;

#define STR(x) XSTR(x)
#define XSTR(x) #x

#define PRINTI(name) { cout<<#name<<"="<<name<<endl; }
#define PRINTS(name) { cout<<#name<<"="<<STR(name)<<endl; }

#ifndef OPS_BETWEEN_TIME_CHECKS
#define OPS_BETWEEN_TIME_CHECKS 500
#endif

#ifdef USE_DEBUGCOUNTERS
    #define GET_COUNTERS ds->debugGetCounters()
    #define CLEAR_COUNTERS ds->clearCounters();
#else
    #define GET_COUNTERS 
    #define CLEAR_COUNTERS 
#endif

pthread_barrier_t WaitForThreads;

struct thread_arg {
    pthread_t* thread;
    int cpu_id;
    int tid;
};

void print_atomic(string coutstr) {
    stringstream ss;
    ss<<coutstr;
    cout<<ss.str();
}

void thread_prefill(int tid, int desg=0, void* t_inf_void=nullptr) {
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;
    test_type garbage = 0;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #elif defined(CBPQ)
    ThrInf* t_inf = (ThrInf*)t_inf_void;
   #endif

    int cnt;
    if (desg) {
        cnt = (PREFILL_AMT / (desg)); // desg = total number of inserters
    } else {
        cnt = (PREFILL_AMT / (THREADS));
    }
   
    while (cnt > 0) {
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;
        //COUTATOMIC("Inserting " << key << "\n");
        GSTATS_TIMER_RESET(tid, timer_latency);
       #ifdef ARRAY_SKIPLIST
        me->op_begin();
       #endif
       #ifdef CBPQ
        if (INSERT_AND_CHECK_SUCCESS_CBPQ) {
       #else 
        if (INSERT_AND_CHECK_SUCCESS) {
       #endif
       //COUTATOMIC("inserting (" << key << "," << value << ")\n");
        
            GSTATS_ADD(tid, key_checksum, key);
            GSTATS_ADD(tid, prefill_size, 1);
            cnt--;
  #ifdef USE_DEBUGCOUNTERS
            glob.keysum->add(tid, key);
            glob.prefillSize->add(tid, 1);
            GET_COUNTERS->insertSuccess->inc(tid);
            
        } else {
            COUTATOMIC("failed to insert\n");
            GET_COUNTERS->insertFail->inc(tid);
  #endif
        }
       #ifdef ARRAY_SKIPLIST
        me->op_end();
       #endif
    }
    glob.__garbage += garbage;
}

void *phased_num_ops(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

    #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
    #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);

    pthread_barrier_wait(&WaitForAll);
    thread_prefill(tid);
    pthread_barrier_wait(&WaitForAll);


    if (tid == 0) {
    #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
        #ifdef TRACK_COUNTERS
        ds->clearCounters();
        #endif
    #endif
        glob.total_running = THREADS;
    }
    
    pthread_barrier_wait(&WaitForAll);

    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);

    // go through the varous phases
    for(int i = 0; i < glob.num_phases; i++) {
        int percent_insertions = glob.phased_workload[i][0];
        int num_ops = glob.phased_workload[i][1] / THREADS;
        //COUTATOMICTID("percent insertions: " << percent_insertions << ", num ops: " << num_ops << endl);
        while (num_ops > 0) {
            //if (((num_ops % 100000) == 0)) COUTATOMICTID("op# "<<num_ops<<endl);
            
            int key = rng->nextNatural(MAXKEY) + 1;
            long long value = rng->nextNatural(MAXKEY) + 1;;
            double op = rng->nextNatural(100000000) / 1000000.;
            if (op < percent_insertions) {
                GSTATS_TIMER_RESET(tid, timer_latency);
                if (INSERT_AND_CHECK_SUCCESS) {
                    GSTATS_ADD(tid, key_checksum, key);
        #ifdef USE_DEBUGCOUNTERS
                    glob.keysum->add(tid, key);
                    GET_COUNTERS->insertSuccess->inc(tid);
                } else {
                    GET_COUNTERS->insertFail->inc(tid);
        #endif
                }
                GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
                GSTATS_ADD(tid, num_inserts, 1);
            } else {
                GSTATS_TIMER_RESET(tid, timer_latency);
            #ifdef LINDEN
                int min_key, min_val;
            #elif defined(SMQ)
                long min_key, min_val;
            #else
                long unsigned min_key, min_val;
            #endif
                DELETE_AND_CHECK_SUCCESS;
                GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
                if (min_key > 0) {
                    GSTATS_ADD(tid, key_checksum, -min_key);
                }
                GSTATS_ADD(tid, num_delmin, 1);
            }
            GSTATS_ADD(tid, num_operations, 1);
            num_ops--;
        }
        glob.total_running.fetch_add(-1);
        while (glob.total_running.load()) { }
        pthread_barrier_wait(&WaitForAll); // make sure all threads have seen total_running == 0 before changing it back
        if (tid == 0) {
            //COUTATOMICTID("here now!" << endl);
            glob.timed_per_phased[i] = chrono::high_resolution_clock::now();
            //ds->getSize();
            glob.total_running = THREADS;
        }
    }
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *mixed_num_ops(void *arg) {
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

    #ifdef CBPQ
    ThrInf *t_inf = (ThrInf *)arg;
    t_inf->pq = ds;
    t_inf->num = THREADS;
    int tid = t_inf->id;
    // todo: need ->values?
    if (tid == 0) {
        COUTATOMIC("init t_inf\n");
    }
    
    #else 
    int tid = *((int*) arg);
    #endif

    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    
    int ops_success = OPS_PER_THREAD;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif

    // int num_keys = 5;
    // int keys[num_keys];
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);

    pthread_barrier_wait(&WaitForAll);
    if (tid == 0) {
        COUTATOMIC("prefilling...\n");
    }
    #ifdef CBPQ
    thread_prefill(tid, 0, t_inf);
    #else
    thread_prefill(tid);
    #endif

    pthread_barrier_wait(&WaitForAll);

    if (tid == 0) {
        COUTATOMIC("done prefilling.\n");
    }

    bool desg = false;
    #if defined(PIPQ_STRICT_DESG_ROLE)
    if (tid == 0) {
        desg = true;
    }
    #endif

    #ifdef CBPQ
    if (tid == 0) {
        //ds->print();
        int keysum_ = ds->debugKeySum();
        COUTATOMIC("keysum after prefill: " << keysum_ << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
    #endif

  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif

    #ifdef ARRAY_SKIPLIST
    if (tid == 0) {
        ds->clear_aborts_aft_prefill();
    }
    pthread_barrier_wait(&WaitForAll);
    #endif

    

    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);

    while ((ops_success > 0) /*|| (tid == 0 && glob.running.load() > 1)*/) {
        VERBOSE if (ops_success&&((ops_success % 500) == 0)) COUTATOMICTID("op# "<<ops_success<<endl);
        // if (ops_success&&((ops_success % 50000) == 0)) COUTATOMICTID("op# "<<ops_success<<endl);
        // if (ops_success < 10) COUTATOMICTID("op# "<<ops_success<<endl);

        if (desg) {
            #ifdef PIPQ_STRICT_DESG_ROLE
            DESG_TD_ROLE;
            #endif
            continue;
        } else {
            double op = rng->nextNatural(100000000) / 1000000.;
          #ifdef ARRAY_SKIPLIST
            me->op_begin();
          #endif
            if (op < INS) {
                int key = rng->nextNatural(MAXKEY) + 1;
                long long value = rng->nextNatural(MAXKEY) + 1;
                GSTATS_TIMER_RESET(tid, timer_latency);
              #ifdef CBPQ
                t_inf->key = key;
                //COUTATOMIC("inserting " << key << "\n");
                if (INSERT_AND_CHECK_SUCCESS_CBPQ) {
              #else
                if (INSERT_AND_CHECK_SUCCESS) {
              #endif
                    GSTATS_ADD(tid, key_checksum, key);
                    //ops_success--;
                }
                GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
                GSTATS_ADD(tid, num_inserts, 1);
            } else {
                GSTATS_TIMER_RESET(tid, timer_latency);
            #if defined(LINDEN) || defined(CBPQ)
                int min_key, min_val;
            #elif defined(SMQ)
                long min_key, min_val;
            #else
                long unsigned min_key, min_val;
            #endif
                #ifdef CBPQ 
                DELETE_AND_CHECK_SUCCESS_CBPQ;
                #else
                DELETE_AND_CHECK_SUCCESS;
                #endif
                //ops_success--;
                GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
                if (min_key > 0) {
                    GSTATS_ADD(tid, key_checksum, -min_key);
                }
                if (min_key > MAXKEY) {
                    COUTATOMIC("min_key is " << min_key << "\n");
                }
                //COUTATOMIC("Returned: " << min_key << "\n");
                //COUTATOMIC("Removed " << min_key << "\n");
                //ds->getKeySum();
                GSTATS_ADD(tid, num_delmin, 1);
            }
        #ifdef ARRAY_SKIPLIST
            me->op_end();
        #endif
            ops_success--;
            GSTATS_ADD(tid, num_operations, 1);
        }
    }
    glob.running.fetch_add(-1);
    //COUTATOMIC("tid=" << tid << " DONE\n");
    while (glob.running.load()) { /* wait */ }

    // if (tid == 0) {
    //     for (int i = 0; i < num_keys; i++) {
    //         COUTATOMIC("(" << tid << "): " << keys[i] << "\n");
    //     }
    //     COUTATOMIC("\n");
    // }
    // pthread_barrier_wait(&WaitForAll);
    // if (tid == 1) {
    //     for (int i = 0; i < num_keys; i++) {
    //         COUTATOMIC("(" << tid << "): " << keys[i] << "\n");
    //     }
    //     COUTATOMIC("\n");
    // }
    // pthread_barrier_wait(&WaitForAll);
    // if (tid == 2) {
    //     for (int i = 0; i < num_keys; i++) {
    //         COUTATOMIC("(" << tid << "): " << keys[i] << "\n");
    //     }
    //     COUTATOMIC("\n");
    // }
    // pthread_barrier_wait(&WaitForAll);
    // if (tid == 3) {
    //     for (int i = 0; i < num_keys; i++) {
    //         COUTATOMIC("(" << tid << "): " << keys[i] << "\n");
    //     }
    //     COUTATOMIC("\n");
    // }
    // pthread_barrier_wait(&WaitForAll);

   #ifdef ARRAY_SKIPLIST
    DEINIT_THREAD;
   #endif
    
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *desg_threads_delmin_numa(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);

    pthread_barrier_wait(&WaitForAll);
    thread_prefill(tid);

    if (tid < DELMIN_THREADS_PER_ZONE) {
        COUTATOMIC("(tid) = (" << tid << ") DEL-MIN THREAD\n");
    }
    
    // int thread_idx;
    // int threads_per_zone;

    // #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED)
    // GET_TIDX;
    // GET_T_PER_ZONE;

    // int del_min_per_zone = (double)DELMIN_THREADS_PER_ZONE * (double)(threads_per_zone / 24.);
    // if (thread_idx < del_min_per_zone) {
    //     COUTATOMIC("(tid, idx) = (" << tid << ", " << thread_idx <<  ") delmin per zone: " << del_min_per_zone << "\n");
    // }
    // #else
    // thread_idx = tid % 24; //! assumes we pin threads numa by numa without ht
    // int zone = (tid / 24);
    // if (THREADS > ((zone+1) * 24)) {
    //     threads_per_zone = 24;
    // } else {
    //     threads_per_zone = THREADS - zone * 24;
    // }

    // int del_min_per_zone = (double)DELMIN_THREADS_PER_ZONE * (double)(threads_per_zone / 24.);
    // if (thread_idx < del_min_per_zone) {
    //     COUTATOMIC("(tid, idx) = (" << tid << ", " << thread_idx <<  ") delmin per zone: " << del_min_per_zone << "\n");
    // }
    // #endif
    

    pthread_barrier_wait(&WaitForAll);

  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif

    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);
    int cnt = 0;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;
        //double op = rng->nextNatural(100000000) / 1000000.;
        if (tid < DELMIN_THREADS_PER_ZONE) {
            GSTATS_TIMER_RESET(tid, timer_latency);
            //COUTATOMIC("Delete-min\n");
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            DELETE_AND_CHECK_SUCCESS;
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            //COUTATOMIC("Removed: " << min_key << "\n");
            GSTATS_ADD(tid, num_delmin, 1);
        } else {
            //COUTATOMIC("Inserting " << key << "\n");
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
    #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
            } else {
                GET_COUNTERS->insertFail->inc(tid);
    #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);
        }
        GSTATS_ADD(tid, num_operations, 1);
    }
    
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *mixed_workload_timed_desg_threads(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);

    pthread_barrier_wait(&WaitForAll);
    
    int thread_idx;
    int threads_per_zone;
    int num_zones = THREADS / 24;

    int num_inserters = THREADS - (((float)THREADS / 24.0) * (float)DELMIN_THREADS_PER_ZONE);

  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    GET_TIDX;
    GET_T_PER_ZONE;
  #else
    thread_idx = tid % 24; //! assumes we pin threads numa by numa without ht
    int zone = (tid / 24);
    if (THREADS > ((zone+1) * 24)) {
        threads_per_zone = 24;
    } else {
        threads_per_zone = THREADS - zone * 24;
    }
  #endif

    int del_min_per_zone = (double)DELMIN_THREADS_PER_ZONE * (double)(threads_per_zone / 24.);
    if (thread_idx < del_min_per_zone) {
        COUTATOMIC("(tid, idx) = (" << tid << ", " << thread_idx <<  ") delmin per zone: " << del_min_per_zone << "\n");
    }

    if (tid == 0) {
        COUTATOMIC("threads per zone: " << threads_per_zone << "\n");
        COUTATOMIC("DELMIN_THREADS_PER_ZONE: " << DELMIN_THREADS_PER_ZONE << "\n");
        COUTATOMIC("threads_per_zone / 24: " << (double)(threads_per_zone / 24.) << "\n");
    }

    pthread_barrier_wait(&WaitForAll);

    if (thread_idx >= del_min_per_zone) {
        COUTATOMIC("PREFILLING " << tid << "!\n");
        thread_prefill(tid, num_inserters); // only prefill
    }
    pthread_barrier_wait(&WaitForAll);

  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif

    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);
    int cnt = 0;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;
        //double op = rng->nextNatural(100000000) / 1000000.;
        if (thread_idx < del_min_per_zone) {
            GSTATS_TIMER_RESET(tid, timer_latency);
            //COUTATOMIC("Delete-min\n");
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            DELETE_AND_CHECK_SUCCESS;
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            //COUTATOMIC("Removed: " << min_key << "\n");
            GSTATS_ADD(tid, num_delmin, 1);
        } else {
            //COUTATOMIC("Inserting " << key << "\n");
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
    #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
            } else {
                GET_COUNTERS->insertFail->inc(tid);
    #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);
        }
        GSTATS_ADD(tid, num_operations, 1);
    }
    
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *mixed_workload_timed_desg_threads_lol(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

    #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
    #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif

    INIT_THREAD(tid);

    pthread_barrier_wait(&WaitForAll);

    int num_active_numa = (THREADS + 12) / 24;
    int idx = tid % 24;

    int tot_delmin_desg;
    if (THREADS == 1) {
        tot_delmin_desg = 0;
        COUTATOMIC("NOTE: running desg experiment with threads = 1 (only performing inserts)\n");
    } else if (THREADS == 2) {
        tot_delmin_desg = 1;
    } else if (THREADS % 24 != 0) {
        assert(THREADS == 12); // currently only works for 12 (e.g., not 36)
        if (DELMIN_THREADS_PER_ZONE == 4) {
            tot_delmin_desg = 1;
        } else {
            assert(DELMIN_THREADS_PER_ZONE % 4 == 0);
            tot_delmin_desg = (DELMIN_THREADS_PER_ZONE * THREADS) / 96;
        }
    } else {
        // THREADS % 24 == 0, THREADS > 2
        tot_delmin_desg = (DELMIN_THREADS_PER_ZONE * THREADS) / 96;
        
        // if (THREADS % 24 != 0) {
        //     int mark = THREADS - 12;
        //     if (desg_del_min > 1) {
        //         tot_desg_tds = ((num_active_numa - 1) * desg_del_min) + (desg_del_min / 2);
        //         if (tid >= mark) {
        //             desg_del_min = desg_del_min / 2;
        //         }
        //     } else {
        //         tot_desg_tds = num_active_numa;
        //     }
        // } else {
        //     tot_desg_tds = num_active_numa * desg_del_min;
        // }
        // COUTATOMIC("NUMA-" << (tid / 24) << ": tot_desg_tds = " << tot_desg_tds << "\n");
    }
    // only prefill if it's an inserting thread
    if (tid >= tot_delmin_desg) {
        thread_prefill(tid, THREADS - tot_delmin_desg);
    }

    pthread_barrier_wait(&WaitForAll);
  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid % 24 == 0) {
        int numa = tid / 24;
        COUTATOMIC("NUMA-" << numa <<  ": Threads = " << THREADS << ", DEL-PASSED = " << DELMIN_THREADS_PER_ZONE << ", actual del-min = " << tot_delmin_desg << "\n");
    }
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif

    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);
    int cnt = 0;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;

        if (tid >= tot_delmin_desg) {
            //COUTATOMIC("tid-" << tid << ": performing insert\n");
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
            #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
            } else {
                GET_COUNTERS->insertFail->inc(tid);
            #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);
        } else {
            //COUTATOMIC("tid-" << tid << ": performing delmin\n");
            GSTATS_TIMER_RESET(tid, timer_latency);
            //COUTATOMIC("Delete-min\n");
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            DELETE_AND_CHECK_SUCCESS;
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            //COUTATOMIC("Removed: " << min_key << "\n");
            GSTATS_ADD(tid, num_delmin, 1);
        }
        GSTATS_ADD(tid, num_operations, 1);
    }
    
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *mixed_workload_timed_desg_threads_one_numa(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

    #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
    #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);

    pthread_barrier_wait(&WaitForAll);
    
    //     int thread_idx;
    //     int threads_per_zone;

    //  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED)
    //     GET_TIDX;
    //     GET_T_PER_ZONE;
    //  #else
    //     thread_idx = tid % 24; //! assumes we pin threads numa by numa without ht
    //     int zone = (tid / 24);
    //     if (THREADS > ((zone+1) * 24)) {
    //         threads_per_zone = 24;
    //     } else {
    //         threads_per_zone = THREADS - zone * 24;
    //     }
    //  #endif
    
    //     int del_min_per_zone = (double)DELMIN_THREADS_PER_ZONE * (double)(threads_per_zone / 24.);
    //     if (tid < del_min_per_zone) {
    //         COUTATOMIC("(tid, idx) = (" << tid << ", " << thread_idx <<  ") delmin per zone: " << del_min_per_zone << "\n");
    //     }

    int num_active_numa = (THREADS + 12) / 24;
    int idx = tid % 24;
    //COUTATOMIC("num_active_numa: " << num_active_numa << "\n");

    int del_min_per_numa = 0; // per NUMA
    if (THREADS == 1) {
        COUTATOMIC("NOTE: running desg experiment with threads = 1 (only performing inserts)\n");
        thread_prefill(tid);
        del_min_per_numa = 0;
    } else if (THREADS == 2) {
        del_min_per_numa = 1;
        if (tid == 1)  { // only inserting thread should prefill
            thread_prefill(tid, THREADS - del_min_per_numa);
        }
    } else if (DELMIN_THREADS_PER_ZONE == 4) {
        del_min_per_numa = 1;
        int tot_desg_tds = num_active_numa;
        //COUTATOMIC("tot_desg_tds = " << tot_desg_tds << "\n");
        if (idx >= del_min_per_numa) {
            thread_prefill(tid, THREADS - tot_desg_tds);
        }
    } else {
        del_min_per_numa = DELMIN_THREADS_PER_ZONE / 4;
        int tot_desg_tds;
        
        if (THREADS % 24 != 0) {
            assert(THREADS % 12 == 0);
            int mark = THREADS - 12;
            if (del_min_per_numa > 1) {
                tot_desg_tds = ((num_active_numa - 1) * del_min_per_numa) + (del_min_per_numa / 2);
                if (tid >= mark) {
                    del_min_per_numa = del_min_per_numa / 2;
                }
            } else {
                tot_desg_tds = num_active_numa;
            }
        } else {
            tot_desg_tds = num_active_numa * del_min_per_numa;
        }

        COUTATOMIC("NUMA-" << (tid / 24) << ": tot_desg_tds = " << tot_desg_tds << "\n");
        
        // only prefill if it's an inserting thread
        if (idx >= del_min_per_numa) {
            thread_prefill(tid, THREADS - tot_desg_tds);
        }
    }

    pthread_barrier_wait(&WaitForAll);
  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid % 24 == 0) {
        int numa = tid / 24;
        COUTATOMIC("NUMA-" << numa <<  ": Threads = " << THREADS << ", DEL-PASSED = " << DELMIN_THREADS_PER_ZONE << ", actual del-min = " << del_min_per_numa << "\n");
    }
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif

    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);
    int cnt = 0;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;

        if (idx >= del_min_per_numa) {
            //COUTATOMIC("tid-" << tid << ": performing insert\n");
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
            #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
            } else {
                GET_COUNTERS->insertFail->inc(tid);
            #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);
        } else {
            //COUTATOMIC("tid-" << tid << ": performing delmin\n");
            GSTATS_TIMER_RESET(tid, timer_latency);
            //COUTATOMIC("Delete-min\n");
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            DELETE_AND_CHECK_SUCCESS;
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            //COUTATOMIC("Removed: " << min_key << "\n");
            GSTATS_ADD(tid, num_delmin, 1);
        }
        GSTATS_ADD(tid, num_operations, 1);
    }
    
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *alternate_workload_timed(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);
    pthread_barrier_wait(&WaitForAll);
    if (tid == 0) {
        COUTATOMIC("Prefilling...\n");
    }
    thread_prefill(tid);
    pthread_barrier_wait(&WaitForAll);

  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif
    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);
    int cnt = 0;
    int op = 1;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        
        if (op == 1) { // INSERT
            int key = rng->nextNatural(MAXKEY) + 1;
            long long value = rng->nextNatural(MAXKEY) + 1;
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
    #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
            } else {
                GET_COUNTERS->insertFail->inc(tid);
    #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);
        } else { // DELETE
            
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            GSTATS_TIMER_RESET(tid, timer_latency);
            DELETE_AND_CHECK_SUCCESS;
            //GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            GSTATS_ADD(tid, num_delmin, 1);
        }
        GSTATS_ADD(tid, num_operations, 1);
        op = -op;
    }
    
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *mixed_workload_timed(void *arg) {
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

    #ifdef CBPQ
    ThrInf *t_inf = (ThrInf *)arg;
    t_inf->pq = ds;
    t_inf->num = THREADS;
    int tid = t_inf->id;
    // todo: need ->values?
    if (tid == 0) {
        COUTATOMIC("init t_inf\n");
    }
    #else 
    int tid = *((int*) arg);
    #endif
    
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif

    bool desg = false;
    #if defined(PIPQ_STRICT_DESG_ROLE)
    if (tid == 0) {
        desg = true;
    }
    #endif

    INIT_THREAD(tid);
    pthread_barrier_wait(&WaitForAll);
    if (tid == 0) {
        COUTATOMIC("Prefilling...\n");
    }
    #ifdef CBPQ
    thread_prefill(tid, 0, t_inf);
    #else
    thread_prefill(tid);
    #endif

    pthread_barrier_wait(&WaitForAll);

  #ifdef CBPQ
    if (tid == 0) {
        //ds->print();
        int keysum_ = ds->debugKeySum();
        COUTATOMIC("keysum after prefill: " << keysum_ << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif

  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(LLSL_TEST) || defined(ARRAY_SKIPLIST) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
       #ifdef ARRAY_SKIPLIST
        std::pair<uint64_t,uint64_t> ret = ds->dump(me);
        long long curr_keysum = ret.second;
        auto size = ret.first;
        ds->clear_aborts_aft_prefill();
       #else
        long long curr_keysum = ds->getKeySum();
        long size = ds->getSize();
       #endif
       COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
       COUTATOMIC("Size: " << size << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif

    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);
    int cnt = 0;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0 || (tid == 0 && glob.running.load() > 1)) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;
        double op = rng->nextNatural(100000000) / 1000000.;
       #ifdef ARRAY_SKIPLIST
        me->op_begin();
       #elif defined(CBPQ)
        t_inf->key = key; // todo: is this nec..?
       #endif

        if (desg) {
          #ifdef PIPQ_STRICT_DESG_ROLE
            DESG_TD_ROLE;
          #endif
          continue;
        } else {
            if (op < INS) {
                GSTATS_TIMER_RESET(tid, timer_latency);
            #ifdef CBPQ
                if (INSERT_AND_CHECK_SUCCESS_CBPQ) {
            #else
                if (INSERT_AND_CHECK_SUCCESS) {
            #endif
                    GSTATS_ADD(tid, key_checksum, key);
        #ifdef USE_DEBUGCOUNTERS
                    glob.keysum->add(tid, key);
                    GET_COUNTERS->insertSuccess->inc(tid);
                } else {
                    GET_COUNTERS->insertFail->inc(tid);
        #endif
                }
                GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
                GSTATS_ADD(tid, num_inserts, 1);
            } else {
                GSTATS_TIMER_RESET(tid, timer_latency);
            #ifdef LINDEN
                int min_key, min_val;
            #elif defined(SMQ) || defined(ARRAY_SKIPLIST)
                long min_key, min_val;
            #else
                long unsigned min_key, min_val;
            #endif
            #ifdef CBPQ 
                DELETE_AND_CHECK_SUCCESS_CBPQ;
            #else
                DELETE_AND_CHECK_SUCCESS;
            #endif
                //GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
                GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
                //COUTATOMIC("Returned: " << min_key << "\n");

                if (min_key > 0) {
                    GSTATS_ADD(tid, key_checksum, -min_key);
                }
                GSTATS_ADD(tid, num_delmin, 1);
            }
            GSTATS_ADD(tid, num_operations, 1);
          #ifdef ARRAY_SKIPLIST
            me->op_end();
          #endif
        }
    }
    
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;

    #ifdef ARRAY_SKIPLIST
    DEINIT_THREAD;
    #endif

    pthread_exit(NULL);
}

void *mixed_workload_every_other_timed(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);
    pthread_barrier_wait(&WaitForAll);
    if (tid == 0) {
        COUTATOMIC("Prefilling...\n");
    }
    thread_prefill(tid);
    pthread_barrier_wait(&WaitForAll);

  #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(LLSL_TEST) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
  #endif
    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);
    int cnt = 0;
    double op = rng->nextNatural(100000000) / 1000000.;
    if (op < 50) {
        cnt = 1;
    }
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;
        if (cnt % 2 == 0) { // INS
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
    #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
            } else {
                GET_COUNTERS->insertFail->inc(tid);
    #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);
        } else { // DEL
            GSTATS_TIMER_RESET(tid, timer_latency);
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            DELETE_AND_CHECK_SUCCESS;
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            GSTATS_ADD(tid, num_delmin, 1);
        }
        GSTATS_ADD(tid, num_operations, 1);
    }
    
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *thread_timed_insert_only(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);
    
    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);

    int cnt = 0;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;

        GSTATS_TIMER_RESET(tid, timer_latency);
        if (INSERT_AND_CHECK_SUCCESS) {
            GSTATS_ADD(tid, key_checksum, key);
    #ifdef USE_DEBUGCOUNTERS
            glob.keysum->add(tid, key);
            GET_COUNTERS->insertSuccess->inc(tid);
        } else {
            GET_COUNTERS->insertFail->inc(tid);
    #endif
        }
        GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
        GSTATS_ADD(tid, num_inserts, 1);
        GSTATS_ADD(tid, num_operations, 1);
        //sleep(1);
    }

    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *insert_only_num_ops(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;
    int ops_success = OPS_PER_THREAD;
    COUTATOMIC("ops per thread: " << ops_success << "\n");

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);
    pthread_barrier_wait(&WaitForAll);
        
    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);

    while (ops_success > 0) {
        VERBOSE if (ops_success&&((ops_success % 500) == 0)) COUTATOMICTID("op# "<<ops_success<<endl);
        
        int key = rng->nextNatural(MAXKEY) + 1;
        long long value = rng->nextNatural(MAXKEY) + 1;

        GSTATS_TIMER_RESET(tid, timer_latency);
        if (INSERT_AND_CHECK_SUCCESS) {
            GSTATS_ADD(tid, key_checksum, key);
            ops_success--;
    #ifdef USE_DEBUGCOUNTERS
            glob.keysum->add(tid, key);
            GET_COUNTERS->insertSuccess->inc(tid);
        } else {
            GET_COUNTERS->insertFail->inc(tid);
    #endif
        }
        
    #ifdef USE_DEBUGCOUNTERS
            glob.keysum->add(tid, key);
            GET_COUNTERS->eraseSuccess->inc(tid);
    #endif

        GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
        GSTATS_ADD(tid, num_inserts, 1);
        GSTATS_ADD(tid, num_operations, 1);
        //sleep(1);
    }

    glob.running.fetch_add(-1);
    while (glob.running.load()) { }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *thread_timed_delete_only(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;
    int ops_success = OPS_PER_THREAD;

    #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
    #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);
    pthread_barrier_wait(&WaitForAll);

    if (tid == 0) {
        COUTATOMIC("Init threads, now going to prefill...\n");
    }

    pthread_barrier_wait(&WaitForAll);
    thread_prefill(tid);
    pthread_barrier_wait(&WaitForAll);
    #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        ds->getSize();
    }
    #endif
        
    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);

    while (ops_success > 0) {
        VERBOSE if (ops_success&&((ops_success % 500) == 0)) COUTATOMICTID("op# "<<ops_success<<endl);
        GSTATS_TIMER_RESET(tid, timer_latency);
    #ifdef LINDEN
        int min_key, min_val;
    #elif defined(SMQ)
        long min_key, min_val;
    #else
        long unsigned min_key, min_val;
    #endif
        DELETE_AND_CHECK_SUCCESS;
        GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
        ops_success--;
    #ifdef USE_DEBUGCOUNTERS
            glob.keysum->add(tid, -min_key);
            GET_COUNTERS->eraseSuccess->inc(tid);
    #endif

        
        GSTATS_ADD(tid, num_delmin, 1);
        GSTATS_ADD(tid, num_operations, 1);
    }
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *thread_timed_insert_least_priority(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);

        pthread_barrier_wait(&WaitForAll);
    if (tid == 0) {
        COUTATOMIC("Prefilling...\n");
    }
    thread_prefill(tid);
    pthread_barrier_wait(&WaitForAll);

    #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
    #endif
    
    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);

    int cnt = 0;
    long last_key = MAXKEY / 2;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        long key = last_key;
        last_key++;
        long long value = rng->nextNatural(MAXKEY) + 1;
        double op = rng->nextNatural(100000000) / 1000000.;

        if (op < INS) {
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
        #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
            } else {
                GET_COUNTERS->insertFail->inc(tid);
        #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);

        } else {
            GSTATS_TIMER_RESET(tid, timer_latency);
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            DELETE_AND_CHECK_SUCCESS;
            //GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            GSTATS_ADD(tid, num_delmin, 1);
        }
        GSTATS_ADD(tid, num_operations, 1);
    }

    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *thread_timed_insert_alternating_priority(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);

    pthread_barrier_wait(&WaitForAll);
    if (tid == 0) {
        COUTATOMIC("Prefilling...\n");
    }
    thread_prefill(tid);
    pthread_barrier_wait(&WaitForAll);

    #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
    #endif
    
    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);

    int cnt = 0;
    long last_key1 = 1;
    long last_key2 = INT_MAX / 2;
    bool key1 = true;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        long key;
        if (key1) {
            key = last_key1;
            last_key1++;
            key1 = false;
        } else {
            key = last_key2;
            last_key2++;
            key1 = true;
        }
        long long value = rng->nextNatural(MAXKEY) + 1;
        double op = rng->nextNatural(100000000) / 1000000.;

        if (op < INS) {
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
        #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
            } else {
                GET_COUNTERS->insertFail->inc(tid);
        #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);

        } else {
            GSTATS_TIMER_RESET(tid, timer_latency);
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            DELETE_AND_CHECK_SUCCESS;
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            GSTATS_ADD(tid, num_delmin, 1);
        }
        GSTATS_ADD(tid, num_operations, 1);
    }
    COUTATOMIC("Keys are: (k1,k2) = (" << last_key1  << "," << last_key2 << ")\n")
    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void *thread_timed_insert_highest_priority_only(void *arg) {
    int tid = *((int*) arg);
    binding_bindThread(tid, LOGICAL_PROCESSORS);
    
    test_type garbage = 0;
    Random *rng = &glob.rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

   #ifdef ARRAY_SKIPLIST
    auto me = new descriptor();
   #endif
    
    #ifdef SPRAY
    unsigned int seed = glob.seeds_[tid];
    #endif
    INIT_THREAD(tid);

    pthread_barrier_wait(&WaitForAll);
    if (tid == 0) {
        COUTATOMIC("Prefilling...\n");
    }
    thread_prefill(tid);
    pthread_barrier_wait(&WaitForAll);

    #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED) || defined(PIPQ_STRICT_ATOMIC)
    if (tid == 0) {
        long long curr_keysum = ds->getKeySum();
        COUTATOMIC("After prefilling, sum: " << curr_keysum << "\n");
        COUTATOMIC("Size: " << ds->getSize() << "\n");
    }
    pthread_barrier_wait(&WaitForAll);
    #endif
    
    papi_create_eventset(tid);
    glob.running.fetch_add(1);
    __sync_synchronize();
    while (!glob.start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    papi_start_counters(tid);

    int cnt = 0;
    int last_key = MAXKEY-1;
    while (!glob.done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-glob.startTime).count() >= abs(MILLIS_TO_RUN)) {
                __sync_synchronize();
                glob.done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = last_key;
        long long value = rng->nextNatural(MAXKEY) + 1;
        last_key--; 
        double op = rng->nextNatural(100000000) / 1000000.;

        if (op < INS) {
            GSTATS_TIMER_RESET(tid, timer_latency);
            if (INSERT_AND_CHECK_SUCCESS) {
                GSTATS_ADD(tid, key_checksum, key);
    #ifdef USE_DEBUGCOUNTERS
                glob.keysum->add(tid, key);
                GET_COUNTERS->insertSuccess->inc(tid);
                
            } else {
                GET_COUNTERS->insertFail->inc(tid);
    #endif
            }
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_updates);
            GSTATS_ADD(tid, num_inserts, 1);
        } else {
            GSTATS_TIMER_RESET(tid, timer_latency);
        #ifdef LINDEN
            int min_key, min_val;
        #elif defined(SMQ)
            long min_key, min_val;
        #else
            long unsigned min_key, min_val;
        #endif
            DELETE_AND_CHECK_SUCCESS;
            GSTATS_TIMER_APPEND_ELAPSED(tid, timer_latency, latency_del);
            if (min_key > 0) {
                GSTATS_ADD(tid, key_checksum, -min_key);
            }
            GSTATS_ADD(tid, num_delmin, 1);
        }
        
        GSTATS_ADD(tid, num_operations, 1);
        //sleep(1);
    }

    glob.running.fetch_add(-1);
    while (glob.running.load()) { /* wait */ }
    
    papi_stop_counters(tid);
    glob.__garbage += garbage;
    pthread_exit(NULL);
}

void trial() {
    //INIT_ALL;
    papi_init_program(THREADS);

    // get random number generator seeded with time
    // we use this rng to seed per-thread rng's that use a different algorithm
    srand(time(NULL));

    // create thread data
    pthread_t *threads[THREADS];
    int ids[THREADS];

    for (int i = 0; i < THREADS; ++i) {
        threads[i] = new pthread_t;
        ids[i] = i;
        glob.rngs[i*PREFETCH_SIZE_WORDS].setSeed(rand());
        glob.seeds_[i] = rand();
    }

    glob.elapsedMillis = 0;
    glob.elapsedMillisNapping = 0;
    glob.start = false;
    glob.done = false;
    glob.running = 0;
    #if defined(NUMA_PQ)
    glob.__ds = (void *) DS_CONSTRUCTOR(THREADS, HEAP_LIST_SIZE);
    #elif defined (PIPQ_STRICT) || defined(PIPQ_STRICT_ATOMIC)
    glob.__ds = (void *) DS_CONSTRUCTOR(HEAP_LIST_SIZE, LEADER_BUFFER_CAP, LEADER_BUFFER_IDEAL_SIZE, THREADS, COUNTER_TSH, COUNTER_MX, LMAX_OFFSET);
    #elif defined (PIPQ_RELAXED)
    glob.__ds = (void *) DS_CONSTRUCTOR(HEAP_LIST_SIZE, THREADS, COUNTER_TSH, COUNTER_MX);
    #elif defined (LLSL_TEST)
    glob.__ds = (void *) DS_CONSTRUCTOR(HEAP_LIST_SIZE, LEADER_BUFFER_CAP, LEADER_BUFFER_IDEAL_SIZE, THREADS, COUNTER_TSH, COUNTER_MX);
    #elif defined(SPRAY)
    glob.__ds = (void *) DS_CONSTRUCTOR(THREADS, MAXKEY);
    #elif defined (LINDEN)
    int offset = 32;
    int max_level = 32;
    glob.__ds = (void *) DS_CONSTRUCTOR(offset, max_level, THREADS);
    #elif defined (LOTAN)
    glob.__ds = (void *) DS_CONSTRUCTOR(THREADS, MAXKEY);
    //seeds = seed_rand();
    #elif defined (SMQ)
    glob.__ds = (void *) DS_CONSTRUCTOR(THREADS);
    #elif defined (MOUNDS)
    glob.__ds = (void *) DS_CONSTRUCTOR();
    #elif defined(ARRAY_SKIPLIST)
    // todo: define optstm stuff, and config
    auto *me = new descriptor();
    glob.__ds = (void *) DS_CONSTRUCTOR(me);
    #elif defined(CBPQ)
    glob.__ds = (void *) DS_CONSTRUCTOR();
    #endif
    
    glob.prefillIntervalElapsedMillis = 0;
    glob.prefillKeySum = 0;
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;
    
    INIT_ALL;

    // amount of time for main thread to wait for children threads
    timespec tsExpected;
    tsExpected.tv_sec = MILLIS_TO_RUN / 1000;
    tsExpected.tv_nsec = (MILLIS_TO_RUN % 1000) * ((__syscall_slong_t) 1000000);
    // short nap
    timespec tsNap;
    tsNap.tv_sec = 0;
    tsNap.tv_nsec = 1000000; // 10mss

  #ifdef CBPQ
    for (int i = 0; i < THREADS; ++i) {
        glob.infos[i].id = i;
        if (BENCHMARK == 3) {
            if (pthread_create(threads[i], NULL, mixed_workload_timed, &(glob.infos[i]))) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 4) {
            if (pthread_create(threads[i], NULL, mixed_num_ops, &(glob.infos[i]))) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        }
    }
  #else
    for (int i = 0; i < THREADS; ++i) {
        if (BENCHMARK == 0) {
            if (pthread_create(threads[i], NULL, thread_timed_insert_only, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 1) {
            if (pthread_create(threads[i], NULL, thread_timed_delete_only, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 2) {
            if (pthread_create(threads[i], NULL, insert_only_num_ops, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 3) {
        //   #ifdef CBPQ
        //     if (pthread_create(threads[i], NULL, mixed_workload_timed, &ids[i], &(glob.infos[i]))) {
        //         cerr<<"ERROR: could not create thread"<<endl;
        //         exit(-1);
        //     }
        //   #else
            if (pthread_create(threads[i], NULL, mixed_workload_timed, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
          //#endif
        } else if (BENCHMARK == 4) {
            if (pthread_create(threads[i], NULL, mixed_num_ops, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 5) {
            if (pthread_create(threads[i], NULL, phased_num_ops, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 6) {
            if (pthread_create(threads[i], NULL, mixed_workload_timed_desg_threads, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            } 
        } else if (BENCHMARK == 61) {
            if (pthread_create(threads[i], NULL, desg_threads_delmin_numa, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 7) {
            if (pthread_create(threads[i], NULL, alternate_workload_timed, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 8) {
            if (pthread_create(threads[i], NULL, mixed_workload_timed_desg_threads_one_numa, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        } else if (BENCHMARK == 9) { 
            if(pthread_create(threads[i], NULL, thread_timed_insert_highest_priority_only, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl; 
                exit(-1); 
            }
        } else if (BENCHMARK == 10) {
            if(pthread_create(threads[i], NULL, thread_timed_insert_least_priority, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl; 
                exit(-1); 
            }
        } else if (BENCHMARK == 11) {
            if(pthread_create(threads[i], NULL, thread_timed_insert_alternating_priority, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl; 
                exit(-1); 
            }
        } else if (BENCHMARK == 12) {
            if(pthread_create(threads[i], NULL, mixed_workload_every_other_timed, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl; 
                exit(-1); 
            }
        } else if (BENCHMARK == 13) {
            if (pthread_create(threads[i], NULL, mixed_workload_timed_desg_threads_lol, &ids[i])) {
                cerr<<"ERROR: could not create thread"<<endl;
                exit(-1);
            }
        }
    }
  #endif

    while (glob.running.load() < THREADS) {
        TRACE COUTATOMIC("main thread: waiting for threads to START running="<<glob.running.load()<<endl);
    } // wait for all threads to be ready
    COUTATOMIC("main thread: starting timer..."<<endl);
    
    COUTATOMIC(endl);
    COUTATOMIC("###############################################################################"<<endl);
    COUTATOMIC("################################ BEGIN RUNNING ################################"<<endl);
    COUTATOMIC("###############################################################################"<<endl);
    COUTATOMIC(endl);
    
    SOFTWARE_BARRIER;

    glob.startTime = chrono::high_resolution_clock::now(); // chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
    __sync_synchronize();
    glob.start = true;
    SOFTWARE_BARRIER;

    // pthread_join is replaced with sleeping, and kill threads if they run too long
    // method: sleep for the desired time + a small epsilon,
    //      then check "running.load()" to see if we're done.
    //      if not, loop and sleep in small increments for up to 5s,
    //      and exit(-1) if running doesn't hit 0.

    if (BENCHMARK == 0 || BENCHMARK == 3 || BENCHMARK == 6 || BENCHMARK == 7 || BENCHMARK == 9 || BENCHMARK == 10 || BENCHMARK == 11 || BENCHMARK == 12 ||  BENCHMARK == 61 ||  BENCHMARK == 13) {
        if (MILLIS_TO_RUN > 0) {
            nanosleep(&tsExpected, NULL);
            SOFTWARE_BARRIER;
            glob.done = true;
            __sync_synchronize();
        }

        const long MAX_NAPPING_MILLIS = (MILLIS_TO_RUN > 0 ? 5000 : 30000);
        glob.elapsedMillis = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - glob.startTime).count();
        glob.elapsedMillisNapping = 0;
        while (glob.running.load() > 0 && glob.elapsedMillisNapping < MAX_NAPPING_MILLIS) {
            nanosleep(&tsNap, NULL);
            glob.elapsedMillisNapping = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - glob.startTime).count() - glob.elapsedMillis;
        }
        if (glob.running.load() > 0) {
            COUTATOMIC(endl);
            COUTATOMIC("Validation FAILURE: "<<glob.running.load()<<" non-terminating thread(s) [did we exhaust physical memory and experience excessive slowdown due to swap mem?]"<<endl);
            COUTATOMIC(endl);
            COUTATOMIC("elapsedMillis="<<glob.elapsedMillis<<" elapsedMillisNapping="<<glob.elapsedMillisNapping<<endl);
            exit(-1);
        }
    } else {
        while (glob.running.load() > 0) {
            nanosleep(&tsNap, NULL);
        }
        glob.endTime = chrono::high_resolution_clock::now();
    }

    __sync_synchronize();
    // join all threads
    for (int i = 0; i < THREADS; ++i) {
        COUTATOMIC("joining thread "<<i<<endl);
        if (pthread_join(*(threads[i]), NULL)) {
            cerr<<"ERROR: could not join thread"<<endl;
            exit(-1);
        }
    }
    __sync_synchronize();
    
    COUTATOMIC(endl);
    COUTATOMIC("###############################################################################"<<endl);
    COUTATOMIC("################################# END RUNNING #################################"<<endl);
    COUTATOMIC("###############################################################################"<<endl);
    COUTATOMIC(endl);
    if (BENCHMARK == 0 || BENCHMARK == 3 || BENCHMARK == 6 || BENCHMARK == 7 || BENCHMARK == 12 || BENCHMARK == 61 || BENCHMARK == 13) {
        COUTATOMIC(((glob.elapsedMillis+glob.elapsedMillisNapping)/1000.)<<"s"<<endl);
    }

    // #ifdef CBPQ
    // ds->print();
    // #endif

    papi_deinit_program();
    DEINIT_ALL;
   
    for (int i = 0; i < THREADS; ++i) {
        delete threads[i];
    }
}

// For BENCHMARK = 5
void parse_mixed_workload(std::string workload) {
    // str = "N,[ins:ops,]+"; N = number of phases
    // e.g., str = "5,100:5000000,50:2500000,95:1000000,100:2000000,95:1000000"
    int idx = workload.find(",");
    glob.num_phases = stoi(workload.substr(0, idx)); // get N
    workload.erase(0, idx + 1); // "100:5000000,50:2500000,95:1000000,100:2000000,95:1000000"

    int cur_i = 0;
    while (workload.length() > 0) {
        idx = workload.find(",");
        std::string cur_load;
        if (idx != std::string::npos) {
            cur_load = workload.substr(0, idx); // "100:5000000"
        } else {
            cur_load = workload;
            idx = workload.length();
        }
        int idx_mid = cur_load.find(":");
        int insertions = stoi(cur_load.substr(0, idx_mid));
        int ops = stoi(cur_load.substr(idx_mid + 1, cur_load.length()));

        if (insertions > 100 or insertions < 0) {
            std::cout << "ERROR: Insertions must be between 0-100 (inclusive).\n";
            exit(0);
        }
        glob.phased_workload[cur_i][0] = insertions;
        glob.phased_workload[cur_i][1] = ops;
        cur_i++;
        
        workload.erase(0, idx + 1);
    }
}

void printOutput() {
    cout<<"PRODUCING OUTPUT"<<endl;
    DS_DECLARATION * ds = (DS_DECLARATION *) glob.__ds;

#ifdef USE_GSTATS
    GSTATS_PRINT;
    cout<<endl;
#endif
    
    long long threadsKeySum = 0;
#ifdef USE_DEBUGCOUNTERS
    {
        threadsKeySum = glob.keysum->getTotal();
        long long dsKeySum = ds->debugKeySum();
        if (threadsKeySum == dsKeySum) {
            cout<<"Validation OK: threadsKeySum = "<<threadsKeySum<<" dsKeySum="<<dsKeySum<<endl;
        } else {
            cout<<"Validation FAILURE: threadsKeySum = "<<threadsKeySum<<" dsKeySum="<<dsKeySum<<endl;
            //exit(-1);
        }
    }
#endif

    #ifdef ARRAY_SKIPLIST
    std::pair<uint64_t,uint64_t> ret;
    #endif
    
#ifdef USE_GSTATS
    {
        threadsKeySum = GSTATS_GET_STAT_METRICS(key_checksum, TOTAL)[0].sum;// + glob.prefillKeySum;
        //COUTATOMIC("glob prefillKeySum: " << glob.prefillKeySum << "\n");
        #ifdef ARRAY_SKIPLIST
        auto me = new descriptor();
        ret = ds->dump(me);
        long long dsKeySum = ret.second;
        #else
        long dsKeySum = ds->debugKeySum();
        #endif
        bool validated = ds->getValidated();
        
        if (threadsKeySum == dsKeySum) {
            cout<<"Validation OK: :) threadsKeySum = "<<threadsKeySum<<" dsKeySum="<<dsKeySum<<endl;
        } else {
            cout<<"Validation FAILURE: :( threadsKeySum = "<<threadsKeySum<<" dsKeySum="<<dsKeySum<<endl;
        }

        if (validated) {
            cout << "Validation OK: :) Traversal validated!\n\n";
        } else {
            cout << "Validation FAILURE: :( Traversal NOT validated\n\n";
        }
    }
#endif
    
    // if (ds->validate(threadsKeySum, true)) {
    //     cout<<"Structural validation OK"<<endl;
    // } else {
    //     cout<<"Structural validation FAILURE."<<endl;
    //     exit(-1);
    // }

    long long totalAll = 0;

#ifdef USE_DEBUGCOUNTERS
    debugCounters * const counters = GET_COUNTERS;
    {
        
        const long long totalInsertions = counters->insertSuccess->getTotal() + counters->insertFail->getTotal();
        const long long totalDeletions = counters->eraseSuccess->getTotal() + counters->eraseFail->getTotal();
        const long long totalUpdates = totalInsertions + totalDeletions;

        const double SECONDS_TO_RUN = (MILLIS_TO_RUN)/1000.;
        const long long throughputIns = (long long) (totalInsertions / SECONDS_TO_RUN);
        const long long throughputDel = (long long) (totalDeletions / SECONDS_TO_RUN);
        const long long throughputAll = (long long) (totalUpdates / SECONDS_TO_RUN);
        COUTATOMIC(endl);
        COUTATOMIC("total insertions              : "<<totalInsertions<<endl);
        COUTATOMIC("total remove min              : "<<totalDeletions<<endl);
        COUTATOMIC("total updates                 : "<<totalUpdates<<endl);
        COUTATOMIC("insertion throughput          : "<<throughputIns<<endl);
        COUTATOMIC("deletion throughput           : "<<throughputDel<<endl);
        COUTATOMIC("total throughput              : "<<throughputAll<<endl);
        COUTATOMIC(endl);
    }
#endif

#ifdef USE_GSTATS
    {
        double SECONDS_TO_RUN;

    //   #if defined(PIPQ_STRICT) || defined(PIPQ_RELAXED)
    //     COUTATOMIC(endl);
    //     float avg_delmin_ops;
    //     GET_AVG_DELMIN_OPS;
    //     COUTATOMIC("avg combiner ops              : "<<avg_delmin_ops<<endl<<endl);
    //   #endif
        
        if (BENCHMARK == 5) {
            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(glob.endTime - glob.startTime).count();
            glob.elapsedMillis = milliseconds;
            SECONDS_TO_RUN = (milliseconds)/1000.;
            COUTATOMIC(SECONDS_TO_RUN << "s\n");

            auto prev_end_time = glob.startTime;
            int total_ops = 0;
            for (int i = 0; i < glob.num_phases; i++) {
                auto end_time_phase = glob.timed_per_phased[i];
                auto ms_phase = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_phase - prev_end_time).count();
                int num_ops_phase = glob.phased_workload[i][1];

                long thpt_phase = (num_ops_phase / float((float)ms_phase / 1000.));
                COUTATOMIC("PHASE" + to_string(i) + " throughput: " + to_string(thpt_phase) + "\t (time = " + to_string(ms_phase) + ")\n");
                
                total_ops += num_ops_phase;
                prev_end_time = end_time_phase;
            }
        } else if (BENCHMARK == 1 || BENCHMARK == 2 || BENCHMARK == 4) {
            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(glob.endTime - glob.startTime).count();
            glob.elapsedMillis = milliseconds;
            SECONDS_TO_RUN = (milliseconds)/1000.;
            COUTATOMIC(SECONDS_TO_RUN << "s\n");
        } else {
            SECONDS_TO_RUN = (MILLIS_TO_RUN)/1000.;
            COUTATOMIC(SECONDS_TO_RUN << "s\n");
        }

        const long long totalDelMin = GSTATS_GET_STAT_METRICS(num_delmin, TOTAL)[0].sum;
        const long long totalInserts = GSTATS_GET_STAT_METRICS(num_inserts, TOTAL)[0].sum;
        const long long totalUpdates = totalDelMin + totalInserts;
        const long long totalOps = GSTATS_GET_STAT_METRICS(num_operations, TOTAL)[0].sum;
        assert(totalOps == totalUpdates);

        const long long totLatency = GSTATS_GET_STAT_METRICS(latency_updates, TOTAL)[0].sum;
        const long long totLatencyDel = GSTATS_GET_STAT_METRICS(latency_del, TOTAL)[0].sum;

        long long insLatAvg = 0;
        long long delLatAvg = 0;
        if (totalInserts > 0)insLatAvg = (long long) (totLatency / totalInserts);
        if (totalDelMin > 0) delLatAvg = (long long) (totLatencyDel / totalDelMin);

        COUTATOMIC(endl);
        COUTATOMIC("total latency ins              : "<<totLatency<<endl);
        COUTATOMIC("total latency del              : "<<totLatencyDel<<endl<<endl);
        COUTATOMIC("latency ins/op                 : "<<insLatAvg<<endl);
        COUTATOMIC("latency del/op                 : "<<delLatAvg<<endl);
        
        const long long throughputUpdates = (long long) (totalUpdates / SECONDS_TO_RUN);
        const long long throughputDelmin = (long long) (totalDelMin / SECONDS_TO_RUN);
        const long long throughputInserts = (long long) (totalInserts / SECONDS_TO_RUN);

        COUTATOMIC(endl);
        COUTATOMIC("total insertions              : "<<totalInserts<<endl);
        COUTATOMIC("total delmin                  : "<<totalDelMin<<endl);
        COUTATOMIC("total updates                 : "<<totalUpdates<<endl<<endl);
        COUTATOMIC("insert throughput             : "<<throughputInserts<<endl);
        COUTATOMIC("delmin throughput             : "<<throughputDelmin<<endl);
        COUTATOMIC("update throughput             : "<<throughputUpdates<<endl<<endl);
        #if defined(PIPQ_STRICT) || defined(LLSL_TEST) || defined(PIPQ_STRICT_ATOMIC)
        string numMoves = ds->getNumMoves();
        string numIns = ds->getNumIns();
        string numFast = ds->getNumFastPath();
        string numCoordUp = ds->getCoordUpsert();
        string numHelping = ds->getNumUps();
        string numTrav = ds->getNumTraversed();
        COUTATOMIC("numMoves: " << numMoves << ", numIns: " << numIns << ", numFast: " << numFast << "\n");
        long summmmm = std::stol(numMoves) + std::stol(numIns) + std::stol(numFast);

        float percent_slowest = 0.0;
        float percent_slow = 0.0;
        float percent_fast = 0.0;

        if (summmmm > 0) {
            percent_slowest = (float(std::stol(numMoves)) / summmmm) * 100;
            percent_slow = (float(std::stol(numIns)) / summmmm) * 100;
            percent_fast = (float(std::stol(numFast)) / summmmm) * 100;
        }
        

        COUTATOMIC("slowEST-path                  : "<<std::to_string(percent_slowest)<<endl);
        COUTATOMIC("slow-path                     : "<<std::to_string(percent_slow)<<endl);
        COUTATOMIC("fast-path                     : "<<std::to_string(percent_fast)<<endl);
        COUTATOMIC("# (sum)                       : "<<to_string(summmmm)<<endl);
        COUTATOMIC("# helping                     : "<<numHelping<<endl<<endl);
        COUTATOMIC("avg # nodes traversed         : "<<numTrav<<endl);
        COUTATOMIC("# coord upsert                : "<<numCoordUp<<endl<<endl);

        COUTATOMIC("Thpt Slowest  Slow  Fast  Helping  Traversed  Coord-Up Lat-INS Lat-DEL\n");
        COUTATOMIC(throughputUpdates << " " << numMoves << " " << numIns << " " << numFast << " " << numHelping << " " << numTrav << " " << numCoordUp << " " << insLatAvg << " " << delLatAvg << "\n");
        

        #endif
        COUTATOMIC(endl);
        
    }
#endif

    long ds_size = GSTATS_GET_STAT_METRICS(prefill_size, TOTAL)[0].sum + GSTATS_GET_STAT_METRICS(num_inserts, TOTAL)[0].sum - GSTATS_GET_STAT_METRICS(num_delmin, TOTAL)[0].sum;
    
    COUTATOMIC("elapsed milliseconds          : "<<glob.elapsedMillis<<endl);
    COUTATOMIC("napping milliseconds overtime : "<<glob.elapsedMillisNapping<<endl);
    #ifdef ARRAY_SKIPLIST
    COUTATOMIC("data structure size count     : "<<ret.first<<endl);
    #else
    COUTATOMIC("data structure size count     : "<<ds->getSizeString()<<endl);
    #endif
    COUTATOMIC("data structure size stats     : "<<ds_size<<endl);
    COUTATOMIC(endl);
    
#if defined(USE_DEBUGCOUNTERS) || defined(USE_GSTATS)
    cout<<"begin papi_print_counters..."<<endl;
    papi_print_counters(totalAll);
    cout<<"end papi_print_counters."<<endl;
#endif
    
    // free ds
    cout<<"begin delete ds..."<<endl;
    delete ds;
    cout<<"end delete ds."<<endl;
    
#ifdef USE_DEBUGCOUNTERS
    VERBOSE COUTATOMIC("main thread: garbage#=");
    VERBOSE COUTATOMIC(counters->garbage->getTotal()<<endl);
#endif
}

int main(int argc, char** argv) {
    
    // setup default args
    MILLIS_TO_RUN = 3000;
    THREADS = 1;
    INS = 50;
    DEL = 50;
    MAXKEY = 1000000;
    BENCHMARK = 1; // 0: INSERT-ONLY, 1: delete only 
    OPS_PER_THREAD = 1000;
    HEAP_LIST_SIZE = 1000000;
    PREFILL_AMT = 1000000;
    LEADERS_PER_SOCKET = 1;
    NUM_ACTIVE_NUMA_ZONES = 1;

    DELMIN_THREADS_PER_ZONE = 1;

    // note: the following are not currently cmd line options, prob should be
    LEADER_BUFFER_CAP = 50;
    LEADER_BUFFER_IDEAL_SIZE = 30;
    COORD_BUFFER_CAP = 20;
    COORD_BUFFER_IDEAL_SIZE = 15;

    COUNTER_TSH = 10;
    COUNTER_MX = 20;
    
    // read command line args
    // example args: -i 50 -d 50 -k 10000 -o 100000 -p -b 0 -t 3000 -n 1 -bind 0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,65,69,73,77,81,85,89,93,97,101,105,109,113,117,121,125,129,133,137,141,145,149,153,157,161,165,169,173,177,181,185,189,2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,98,102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,178,182,186,190,3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63,67,71,75,79,83,87,91,95,99,103,107,111,115,119,123,127,131,135,139,143,147,151,155,159,163,167,171,175,179,183,187,191
    for (int i = 1; i<argc; ++i) {
        if (strcmp(argv[i], "-i") == 0) {
            INS = atof(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0) {
            DEL = atof(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0) {
            THREADS = atof(argv[++i]);
        } else if (strcmp(argv[i], "-k") == 0) {
            MAXKEY = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0) {
            MILLIS_TO_RUN = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0) {
            BENCHMARK = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0) {
            OPS_PER_THREAD = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            HEAP_LIST_SIZE = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-lc") == 0) {
            LEADER_BUFFER_CAP = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-li") == 0) {
            LEADER_BUFFER_IDEAL_SIZE = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-cc") == 0) {
            COORD_BUFFER_CAP =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-ci") == 0) {
            COORD_BUFFER_IDEAL_SIZE =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0) {
            PREFILL_AMT =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0) {
            LEADERS_PER_SOCKET =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-az") == 0) {
            NUM_ACTIVE_NUMA_ZONES =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-dz") == 0) {
            DELMIN_THREADS_PER_ZONE =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-ct") == 0) {
            COUNTER_TSH =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-cm") == 0) {
            COUNTER_MX =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0) {
            LMAX_OFFSET =  atoi(argv[++i]);
        } else if (strcmp(argv[i], "-bind") == 0) { // e.g., "-bind 1,2,3,8-11,4-7,0"
            binding_parseCustom(argv[++i]);
            cout << "parsed custom binding: " << argv[i] << endl;
        } else if (strcmp(argv[i], "-phased") == 0) {
            parse_mixed_workload(argv[++i]);
        } else {
            cout<<"bad argument "<<argv[i]<<endl;
            exit(1);
        }
    }

    // ensure they passed a binding policy
    if (numCustomBindings == 0) {
        cout<<"Must pass a binding policy (-bind)"<<endl;
        exit(1);
    }
    if (LEADER_BUFFER_IDEAL_SIZE > LEADER_BUFFER_CAP) {
        cout<<"Ideal size of leader buffer cannot be greater than its capacity."<<endl;
        exit(1);
    }
    if (COORD_BUFFER_IDEAL_SIZE > COORD_BUFFER_CAP) {
        cout<<"Ideal size of coordinator buffer cannot be greater than its capacity."<<endl;
        exit(1);
    }
    if (INS + DEL != 100) {
        cout<<"Insertions \% + Deletions \% must equal 100%"<<endl;
        exit(1);
    } else {
        cout<<"Insertions is " << INS << ", Deletions is "<< DEL << endl;
    }

    if (BENCHMARK == 4) {
        OPS_PER_THREAD = OPS_PER_THREAD / (THREADS);
    }

    pthread_barrier_init(&WaitForAll, NULL, THREADS);
    
    // print used args
    PRINTS(INSERT_FUNC);
    PRINTS(REMOVE_MIN_FUNC);
    PRINTS(RECLAIM);
    PRINTS(ALLOC);
    PRINTS(POOL);
    PRINTI(MILLIS_TO_RUN);
    PRINTI(INS);
    PRINTI(DEL);
    PRINTI(MAXKEY);
    PRINTI(THREADS);
    PRINTI(BENCHMARK);
    PRINTI(OPS_PER_THREAD);
    PRINTI(HEAP_LIST_SIZE);
#ifdef WIDTH_SEQ
    PRINTI(WIDTH_SEQ);
#endif

    std::string DS = "";
  #if defined(PIPQ_STRICT)
    DS = "Us-Lin";
  #elif defined(PIPQ_STRICT_ATOMIC)
    DS = "Us-Lin-Atomics";
  #elif defined(LLSL_TEST)
    DS = "Us-LLSL-Test";
  #elif defined(PIPQ_RELAXED)
    DS = "Us-Rel";
  #elif defined(LINDEN)
    DS = "Linden";
  #elif defined(SPRAY)
    DS = "Spray";
  #elif defined(SMQ)
    DS = "SMQ";
  #elif defined(LOTAN)
    DS = "Lotan";
  #elif defined(MOUNDS)
    DS = "Mounds";
  #elif defined(ARRAY_SKIPLIST)
    DS = "array_skiplist_pq";
  #elif defined(CBPQ)
    DS = "CBPQ";
  #endif
    PRINTI(DS);


    pthread_barrier_init(&WaitForThreads, NULL, THREADS);
    binding_configurePolicy(THREADS, LOGICAL_PROCESSORS);
    
    // print actual thread pinning/binding layout
    cout<<"ACTUAL_THREAD_BINDINGS=";
    for (int i=0;i<THREADS;++i) {
        cout<<(i?",":"")<<binding_getActualBinding(i, LOGICAL_PROCESSORS);
    }
    cout<<endl;
    if (!binding_isInjectiveMapping(THREADS, LOGICAL_PROCESSORS)) {
        cout<<"ERROR: thread binding maps more than one thread to a single logical processor"<<endl;
        exit(-1);
    }
    // setup per-thread statistics
    GSTATS_CREATE_ALL;
    
#ifdef USE_DEBUGCOUNTERS
    // per-thread stats for prefilling and key-checksum validation of the data structure
    glob.keysum = new debugCounter(MAX_TID_POW2);
    glob.prefillSize = new debugCounter(MAX_TID_POW2);
#endif
    
    trial();
    //cout<<"here!\n";
    printOutput();
    
    binding_deinit(LOGICAL_PROCESSORS);
    cout<<"garbage="<<glob.__garbage<<endl; // to prevent certain steps from being optimized out
#ifdef USE_DEBUGCOUNTERS
    delete glob.keysum;
    delete glob.prefillSize;
#endif
    GSTATS_DESTROY;
    return 0;
}