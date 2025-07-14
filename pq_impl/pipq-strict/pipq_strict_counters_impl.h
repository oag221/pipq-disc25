#include <barrier>
#include <cassert>
#include <cmath>
#include "pipq_strict.h"
#include "../recordmgr/debugprinting.h"
#ifndef SSSP
#include "../common/binding.h"
#endif
using namespace std;
using namespace pq_ns;

/*         --------------------------------------------         */
/*                                                              */
/*                      PQ INIT METHODS                         */
/*                                                              */
/*         --------------------------------------------         */

template <class V>
void pq_ns::pq<V>::PQInit() {
    COUTATOMIC("Initializing structures and metadata\n");
    active_numa_zones_0 = (bool*)numa_alloc_onnode(NUMA_ZONES * sizeof(bool), NODE_0);
    thread_mappings_0   = (int*)numa_alloc_onnode(TOTAL_THREADS * sizeof(int), NODE_0);
    counter_0 = (int*)numa_alloc_onnode(NUMA_ZONES * sizeof(int), NODE_0);

    active_numa_zones_1 = (bool*)numa_alloc_onnode(NUMA_ZONES * sizeof(bool), NODE_1);
    thread_mappings_1   = (int*)numa_alloc_onnode(TOTAL_THREADS * sizeof(int), NODE_1);
    counter_1 = (int*)numa_alloc_onnode(NUMA_ZONES * sizeof(int), NODE_1);

    active_numa_zones_2 = (bool*)numa_alloc_onnode(NUMA_ZONES * sizeof(bool), NODE_2);
    thread_mappings_2   = (int*)numa_alloc_onnode(TOTAL_THREADS * sizeof(int), NODE_2);
    counter_2 = (int*)numa_alloc_onnode(NUMA_ZONES * sizeof(int), NODE_2);
    
    active_numa_zones_3 = (bool*)numa_alloc_onnode(NUMA_ZONES * sizeof(bool), NODE_3);
    thread_mappings_3   = (int*)numa_alloc_onnode(TOTAL_THREADS * sizeof(int), NODE_3);
    counter_3 = (int*)numa_alloc_onnode(NUMA_ZONES * sizeof(int), NODE_3);

    // init numa zones to false
    for (int i = 0; i < NUMA_ZONES; i++) {
        active_numa_zones_0[i] = false;
        active_numa_zones_1[i] = false;
        active_numa_zones_2[i] = false;
        active_numa_zones_3[i] = false;
    }

    // num active workers per numa zone
    int cnt0 = 0;
    int cnt1 = 0;
    int cnt2 = 0;
    int cnt3 = 0;

    // init t-local heaps and setup thread mapping (tid -> idx in buffers)
    for (int i = 0; i < TOTAL_THREADS; i++) {
        int cpu_id = i/24 + 4*(i % 24); //! assumes threads are pinned numa by numa
        //int cpu_id = customBinding[i];
        int group = get_group(cpu_id);
        switch(group) {
            case 0:
                thread_mappings_0[i] = cnt0;
                thread_mappings_1[i] = cnt0;
                thread_mappings_2[i] = cnt0;
                thread_mappings_3[i] = cnt0;
                active_numa_zones_0[group] = true;
                active_numa_zones_1[group] = true;
                active_numa_zones_2[group] = true;
                active_numa_zones_3[group] = true;
                cnt0++;
                break;
            case 1:
                thread_mappings_0[i] = cnt1;
                thread_mappings_1[i] = cnt1;
                thread_mappings_2[i] = cnt1;
                thread_mappings_3[i] = cnt1;
                active_numa_zones_0[group] = true;
                active_numa_zones_1[group] = true;
                active_numa_zones_2[group] = true;
                active_numa_zones_3[group] = true;
                cnt1++;
                break;
            case 2:
                thread_mappings_0[i] = cnt2;
                thread_mappings_1[i] = cnt2;
                thread_mappings_2[i] = cnt2;
                thread_mappings_3[i] = cnt2;
                active_numa_zones_0[group] = true;
                active_numa_zones_1[group] = true;
                active_numa_zones_2[group] = true;
                active_numa_zones_3[group] = true;
                cnt2++;
                break;
            case 3:
                thread_mappings_0[i] = cnt3;
                thread_mappings_1[i] = cnt3;
                thread_mappings_2[i] = cnt3;
                thread_mappings_3[i] = cnt3;
                active_numa_zones_0[group] = true;
                active_numa_zones_1[group] = true;
                active_numa_zones_2[group] = true;
                active_numa_zones_3[group] = true;
                cnt3++;
                break;
            default:
                cerr << "Error allocating memory: NUMA node " << group << " does not exist." << endl;
                break;
        }
    }
    // stored to retrieve, numa-locally, the number of workers in each socket
    counter_0[NODE_0] = cnt0;
    counter_1[NODE_0] = cnt0;
    counter_2[NODE_0] = cnt0;
    counter_3[NODE_0] = cnt0;

    counter_0[NODE_1] = cnt1;
    counter_1[NODE_1] = cnt1;
    counter_2[NODE_1] = cnt1;
    counter_3[NODE_1] = cnt1;

    counter_0[NODE_2] = cnt2;
    counter_1[NODE_2] = cnt2;
    counter_2[NODE_2] = cnt2;
    counter_3[NODE_2] = cnt2;

    counter_0[NODE_3] = cnt3;
    counter_1[NODE_3] = cnt3;
    counter_2[NODE_3] = cnt3;
    counter_3[NODE_3] = cnt3;

    num_moves_0 = (DebugCounterSlot*)numa_alloc_onnode(cnt0 * sizeof(DebugCounterSlot), NODE_0);
    num_moves_1 = (DebugCounterSlot*)numa_alloc_onnode(cnt1 * sizeof(DebugCounterSlot), NODE_1);
    num_moves_2 = (DebugCounterSlot*)numa_alloc_onnode(cnt2 * sizeof(DebugCounterSlot), NODE_2);
    num_moves_3 = (DebugCounterSlot*)numa_alloc_onnode(cnt3 * sizeof(DebugCounterSlot), NODE_3);

    coord_pullup_0 = (DebugCounterSlot*)numa_alloc_onnode(cnt0 * sizeof(DebugCounterSlot), NODE_0);
    coord_pullup_1 = (DebugCounterSlot*)numa_alloc_onnode(cnt1 * sizeof(DebugCounterSlot), NODE_1);
    coord_pullup_2 = (DebugCounterSlot*)numa_alloc_onnode(cnt2 * sizeof(DebugCounterSlot), NODE_2);
    coord_pullup_3 = (DebugCounterSlot*)numa_alloc_onnode(cnt3 * sizeof(DebugCounterSlot), NODE_3);

    num_traversed_0 = (DebugCounterSlot*)numa_alloc_onnode(cnt0 * sizeof(DebugCounterSlot), NODE_0);
    num_traversed_1 = (DebugCounterSlot*)numa_alloc_onnode(cnt1 * sizeof(DebugCounterSlot), NODE_1);
    num_traversed_2 = (DebugCounterSlot*)numa_alloc_onnode(cnt2 * sizeof(DebugCounterSlot), NODE_2);
    num_traversed_3 = (DebugCounterSlot*)numa_alloc_onnode(cnt3 * sizeof(DebugCounterSlot), NODE_3);

    num_times_traversed_0 = (DebugCounterSlot*)numa_alloc_onnode(cnt0 * sizeof(DebugCounterSlot), NODE_0);
    num_times_traversed_1 = (DebugCounterSlot*)numa_alloc_onnode(cnt1 * sizeof(DebugCounterSlot), NODE_1);
    num_times_traversed_2 = (DebugCounterSlot*)numa_alloc_onnode(cnt2 * sizeof(DebugCounterSlot), NODE_2);
    num_times_traversed_3 = (DebugCounterSlot*)numa_alloc_onnode(cnt3 * sizeof(DebugCounterSlot), NODE_3);

    num_fastpath_0 = (DebugCounterSlot*)numa_alloc_onnode(cnt0 * sizeof(DebugCounterSlot), NODE_0);
    num_fastpath_1 = (DebugCounterSlot*)numa_alloc_onnode(cnt1 * sizeof(DebugCounterSlot), NODE_1);
    num_fastpath_2 = (DebugCounterSlot*)numa_alloc_onnode(cnt2 * sizeof(DebugCounterSlot), NODE_2);
    num_fastpath_3 = (DebugCounterSlot*)numa_alloc_onnode(cnt3 * sizeof(DebugCounterSlot), NODE_3);

    num_ins_0 = (DebugCounterSlot*)numa_alloc_onnode(cnt0 * sizeof(DebugCounterSlot), NODE_0);
    num_ins_1 = (DebugCounterSlot*)numa_alloc_onnode(cnt1 * sizeof(DebugCounterSlot), NODE_1);
    num_ins_2 = (DebugCounterSlot*)numa_alloc_onnode(cnt2 * sizeof(DebugCounterSlot), NODE_2);
    num_ins_3 = (DebugCounterSlot*)numa_alloc_onnode(cnt3 * sizeof(DebugCounterSlot), NODE_3);

    help_upsert_0 = (DebugCounterSlot*)numa_alloc_onnode(cnt0 * sizeof(DebugCounterSlot), NODE_0);
    help_upsert_1 = (DebugCounterSlot*)numa_alloc_onnode(cnt1 * sizeof(DebugCounterSlot), NODE_1);
    help_upsert_2 = (DebugCounterSlot*)numa_alloc_onnode(cnt2 * sizeof(DebugCounterSlot), NODE_2);
    help_upsert_3 = (DebugCounterSlot*)numa_alloc_onnode(cnt3 * sizeof(DebugCounterSlot), NODE_3);

    // initializing worker heaps, one per NUMA zone (array containing one heap per thread)
    heap_0 = (PQ_Heap**)numa_alloc_onnode(cnt0 * sizeof(PQ_Heap*), NODE_0);
    heap_1 = (PQ_Heap**)numa_alloc_onnode(cnt1 * sizeof(PQ_Heap*), NODE_1);
    heap_2 = (PQ_Heap**)numa_alloc_onnode(cnt2 * sizeof(PQ_Heap*), NODE_2);
    heap_3 = (PQ_Heap**)numa_alloc_onnode(cnt3 * sizeof(PQ_Heap*), NODE_3);
    for (int i = 0; i < cnt0; i++) {
        HeapInit(&(heap_0[i]), NODE_0, HEAP_LIST_SIZE);
    }
    for (int i = 0; i < cnt1; i++) {
        HeapInit(&(heap_1[i]), NODE_1, HEAP_LIST_SIZE);
    }
    for (int i = 0; i < cnt2; i++) {
        HeapInit(&(heap_2[i]), NODE_2, HEAP_LIST_SIZE);
    }
    for (int i = 0; i < cnt3; i++) {
        HeapInit(&(heap_3[i]), NODE_3, HEAP_LIST_SIZE);
    }

    // initialize the leader structure (calls method defined in harris.h)
    leader_set = set_new();

    int total_cnt = cnt0 + cnt1 + cnt2 + cnt3;
    lead_counters_0 = (CounterSlot*)numa_alloc_onnode(cnt0 * sizeof(CounterSlot), NODE_0);
    lead_counters_1 = (CounterSlot*)numa_alloc_onnode(cnt1 * sizeof(CounterSlot), NODE_1);
    lead_counters_2 = (CounterSlot*)numa_alloc_onnode(cnt2 * sizeof(CounterSlot), NODE_2);
    lead_counters_3 = (CounterSlot*)numa_alloc_onnode(cnt3 * sizeof(CounterSlot), NODE_3);

    largest_in_leader_0 = (LeaderLargest*)numa_alloc_onnode(cnt0 * sizeof(LeaderLargest), NODE_0);
    largest_in_leader_1 = (LeaderLargest*)numa_alloc_onnode(cnt1 * sizeof(LeaderLargest), NODE_1);
    largest_in_leader_2 = (LeaderLargest*)numa_alloc_onnode(cnt2 * sizeof(LeaderLargest), NODE_2);
    largest_in_leader_3 = (LeaderLargest*)numa_alloc_onnode(cnt3 * sizeof(LeaderLargest), NODE_3);

    // init leader counters and max ptrs
    for (int i = 0; i < cnt0; i++) {
        lead_counters_0[i].count = 0;
        largest_in_leader_0[i].largest_ptr = NULL;
    }
    for (int i = 0; i < cnt1; i++) {
        lead_counters_1[i].count = 0;
        largest_in_leader_1[i].largest_ptr = NULL;
    }
    for (int i = 0; i < cnt2; i++) {
        lead_counters_2[i].count = 0;
        largest_in_leader_2[i].largest_ptr = NULL;
    }
    for (int i = 0; i < cnt3; i++) {
        lead_counters_3[i].count = 0;
        largest_in_leader_3[i].largest_ptr = NULL;
    }
    
    // used to correct key-sum in case duplicates are encountered at the leader level
    repeat_keys = (volatile long long*)numa_alloc_onnode(total_cnt * sizeof(volatile long long), NODE_0);
    for (int i = 0; i < total_cnt; i++) {
        repeat_keys[i] = 0;
    }

    // coordinator inits
    coord_lock = (volatile long*)numa_alloc_onnode(sizeof(volatile long), NODE_0);
    *coord_lock = 0;

    compete_coord_0 = (volatile long*)numa_alloc_onnode(sizeof(volatile long), NODE_0);
    compete_coord_1 = (volatile long*)numa_alloc_onnode(sizeof(volatile long), NODE_1);
    compete_coord_2 = (volatile long*)numa_alloc_onnode(sizeof(volatile long), NODE_2);
    compete_coord_3 = (volatile long*)numa_alloc_onnode(sizeof(volatile long), NODE_3);
    *compete_coord_0 = 0;
    *compete_coord_1 = 0;
    *compete_coord_2 = 0;
    *compete_coord_3 = 0;

    Announce_allocation(&announce_coord_0, cnt0, NODE_0); // "announce_coord_0" : for coordinator when deleting
    Announce_allocation(&announce_coord_1, cnt1, NODE_1);
    Announce_allocation(&announce_coord_2, cnt2, NODE_2);
    Announce_allocation(&announce_coord_3, cnt3, NODE_3);

    delmin_cntr_0 = (DelMinCntr*)numa_alloc_onnode(cnt0 * sizeof(DelMinCntr), NODE_0);
    delmin_cntr_1 = (DelMinCntr*)numa_alloc_onnode(cnt1 * sizeof(DelMinCntr), NODE_1);
    delmin_cntr_2 = (DelMinCntr*)numa_alloc_onnode(cnt2 * sizeof(DelMinCntr), NODE_2);
    delmin_cntr_3 = (DelMinCntr*)numa_alloc_onnode(cnt3 * sizeof(DelMinCntr), NODE_3);

    COUTATOMIC("Done, moving on to thread inits\n");
}

// set thread local variables
template <class V>
void pq_ns::pq<V>::threadInit(int tid) {
    int cpu_id = tid/24 + 4*(tid % 24);
    //int cpu_id = customBinding[tid];
    t_group = get_group(cpu_id);
    t_tid = tid;
    t_idx = get_thread_mapping(t_group, tid);
    t_local_heap = get_heap_mapping(t_idx, t_group);
    COUTATOMIC("INITIALIZING THREAD (tid, idx): (" << t_tid << ", " << t_idx << ")\tcpu_id = " << cpu_id << "\tCPUID: " << sched_getcpu() << ", zone: " << t_group <<  "\n");

    switch(t_group) {
        case NODE_0:
            announce_coord = announce_coord_0;
            t_num_workers = counter_0[t_group];
            t_compete_coord_lock = compete_coord_0;
            t_lead_counters = &(lead_counters_0[t_idx]);
            t_largest_in_leader = &(largest_in_leader_0[t_idx]);

            t_delmin_cntr = delmin_cntr_0;

            t_num_moves = &(num_moves_0[t_idx]);
            num_moves_0[t_idx].count = 0;
            t_num_ins = &(num_ins_0[t_idx]);
            num_ins_0[t_idx].count = 0;
            t_help_upsert = &(help_upsert_0[t_idx]);
            help_upsert_0[t_idx].count = 0;
            t_num_fastpath = &(num_fastpath_0[t_idx]);
            num_fastpath_0[t_idx].count = 0;

            t_num_traversed = &(num_traversed_0[t_idx]);
            num_traversed_0[t_idx].count = 0;
            t_num_times_traversed = &(num_times_traversed_0[t_idx]);
            num_times_traversed_0[t_idx].count = 0;
            t_coord_pullup = &(coord_pullup_0[t_idx]);
            coord_pullup_0[t_idx].count = 0;

            break;
        case NODE_1:
            announce_coord = announce_coord_1;
            t_num_workers = counter_1[t_group];
            t_compete_coord_lock = compete_coord_1;
            t_lead_counters = &(lead_counters_1[t_idx]);
            t_largest_in_leader = &(largest_in_leader_1[t_idx]);

            t_delmin_cntr = delmin_cntr_1;
            t_num_moves = &(num_moves_1[t_idx]);
            num_moves_1[t_idx].count = 0;
            t_num_ins = &(num_ins_1[t_idx]);
            num_ins_1[t_idx].count = 0;
            t_help_upsert = &(help_upsert_1[t_idx]);
            help_upsert_1[t_idx].count = 0;
            t_num_fastpath = &(num_fastpath_1[t_idx]);
            num_fastpath_1[t_idx].count = 0;

            t_num_traversed = &(num_traversed_1[t_idx]);
            num_traversed_1[t_idx].count = 0;
            t_num_times_traversed = &(num_times_traversed_1[t_idx]);
            num_times_traversed_1[t_idx].count = 0;

            t_coord_pullup = &(coord_pullup_1[t_idx]);
            coord_pullup_1[t_idx].count = 0;
            break;
        case NODE_2:
            announce_coord = announce_coord_2;
            t_num_workers = counter_2[t_group];
            t_compete_coord_lock = compete_coord_2;
            t_lead_counters = &(lead_counters_2[t_idx]);
            t_largest_in_leader = &(largest_in_leader_2[t_idx]);

            t_delmin_cntr = delmin_cntr_2;
            t_num_moves = &(num_moves_2[t_idx]);
            num_moves_2[t_idx].count = 0;
            t_num_ins = &(num_ins_2[t_idx]);
            num_ins_2[t_idx].count = 0;
            t_help_upsert = &(help_upsert_2[t_idx]);
            help_upsert_2[t_idx].count = 0;
            t_num_fastpath = &(num_fastpath_2[t_idx]);
            num_fastpath_2[t_idx].count = 0;
            t_num_traversed = &(num_traversed_2[t_idx]);
            num_traversed_2[t_idx].count = 0;
            t_num_times_traversed = &(num_times_traversed_2[t_idx]);
            num_times_traversed_2[t_idx].count = 0;
            t_coord_pullup = &(coord_pullup_2[t_idx]);
            coord_pullup_2[t_idx].count = 0;
            break;
        case NODE_3:
            announce_coord = announce_coord_3;
            t_num_workers = counter_3[t_group];
            t_compete_coord_lock = compete_coord_3;
            t_lead_counters = &(lead_counters_3[t_idx]);
            t_largest_in_leader = &(largest_in_leader_3[t_idx]);

            t_delmin_cntr = delmin_cntr_3;
            t_num_moves = &(num_moves_3[t_idx]);
            num_moves_3[t_idx].count = 0;
            t_num_ins = &(num_ins_3[t_idx]);
            num_ins_3[t_idx].count = 0;
            t_help_upsert = &(help_upsert_3[t_idx]);
            help_upsert_3[t_idx].count = 0;
            t_num_fastpath = &(num_fastpath_3[t_idx]);
            num_fastpath_3[t_idx].count = 0;
            t_num_traversed = &(num_traversed_3[t_idx]);
            num_traversed_3[t_idx].count = 0;
            t_num_times_traversed = &(num_times_traversed_3[t_idx]);
            num_times_traversed_3[t_idx].count = 0;

            t_coord_pullup = &(coord_pullup_3[t_idx]);
            coord_pullup_3[t_idx].count = 0;
            break;
    }
    pthread_barrier_wait(&WaitForAll);
}

template <class V>
void pq_ns::pq<V>::HeapInit(PQ_Heap** heap, int group, int heap_size) {
    (*heap) = (PQ_Heap*)numa_alloc_onnode(sizeof(PQ_Heap), group);
    (*heap)->pq_ptr = (HeapList*)numa_alloc_onnode(sizeof(HeapList), group);
    (*heap)->pq_ptr->heapList = (PQ_Node*)numa_alloc_onnode(heap_size * sizeof(PQ_Node), group); // TODO: should align size be here???
    (*heap)->pq_ptr->next = nullptr;
    (*heap)->pq_ptr->prev = nullptr;
    (*heap)->lock   = (long*)numa_alloc_onnode(sizeof(long), group);
    *((*heap)->lock) = 0;
    (*heap)->size   = ROOT; // ROOT = 0
}

template <class V>
void pq_ns::pq<V>::Announce_allocation(AnnounceStruct **announce, int size, int group) {
    (*announce) = (AnnounceStruct *)numa_alloc_onnode(size * sizeof(AnnounceStruct), group);
    for (int i = 0; i < size; i++) {
        (*announce)[i].status  = false;
        (*announce)[i].key = EMPTY;
        (*announce)[i].detected = 0;
    } 
}

template <class V>
void pq_ns::pq<V>::PQDeinit() {
    COUTATOMIC("Calculating key sum and data structure size...\n");
    keySum = getKeySum();
    finalSize = getSize();
    numMoves = getTotalMoves();
    numTraversed = getTotalTraversed();
    numIns = getTotalInsertUp();
    numUpsert = getTotalHelpUpsert();
    numFastPath = getTotalFastPath();
    numCoordUpsert = getNumCoordPullUp();
    sumCntDelminOps();
    //validate_insertion_ordering();
    COUTATOMIC("\n\n[[ VALIDATING ORDERING COMMENTED OUT ]]\n");
    COUTATOMIC("Done.\nNow de-init.\n");

    // de-init heap members & mappings
    for (int i = 0; i < TOTAL_THREADS; i++) {
        int cpu_id = i/24 + 4*(i % 24);
        //int cpu_id = customBinding[i];
        int group = get_group(cpu_id);
        int idx = get_thread_mapping(group, i);
        PQ_Heap* heap = get_heap_mapping(idx, group);
        
        // deinit thread-local heap
        HeapDeinit(&heap);
    }
    // de-init the actual NUMA-local heaps
    numa_free(heap_0, counter_0[NODE_0] * sizeof(PQ_Heap));
    numa_free(heap_1, counter_0[NODE_1] * sizeof(PQ_Heap));
    numa_free(heap_2, counter_0[NODE_2] * sizeof(PQ_Heap));
    numa_free(heap_3, counter_0[NODE_3] * sizeof(PQ_Heap));

    int total_cnt = counter_0[NODE_0] + counter_0[NODE_1] + counter_0[NODE_2] + counter_0[NODE_3];
    numa_free(lead_counters_0, counter_0[NODE_0] * sizeof(CounterSlot));
    numa_free(lead_counters_1, counter_0[NODE_1] * sizeof(CounterSlot));
    numa_free(lead_counters_2, counter_0[NODE_2] * sizeof(CounterSlot));
    numa_free(lead_counters_3, counter_0[NODE_3] * sizeof(CounterSlot));

    numa_free(largest_in_leader_0, counter_0[NODE_0] * sizeof(LeaderLargest*));
    numa_free(largest_in_leader_1, counter_0[NODE_1] * sizeof(LeaderLargest*));
    numa_free(largest_in_leader_2, counter_0[NODE_2] * sizeof(LeaderLargest*));
    numa_free(largest_in_leader_3, counter_0[NODE_3] * sizeof(LeaderLargest*));
    
    set_destroy(leader_set);
    numa_free((void*)repeat_keys, total_cnt * sizeof(volatile long long));

    numa_free((void*)coord_lock, sizeof(volatile long));
    numa_free((void*)compete_coord_0, sizeof(volatile long));
    numa_free((void*)compete_coord_1, sizeof(volatile long));
    numa_free((void*)compete_coord_2, sizeof(volatile long));
    numa_free((void*)compete_coord_3, sizeof(volatile long));

    numa_free(thread_mappings_0, TOTAL_THREADS * sizeof(int));
    numa_free(thread_mappings_1, TOTAL_THREADS * sizeof(int));
    numa_free(thread_mappings_2, TOTAL_THREADS * sizeof(int));
    numa_free(thread_mappings_3, TOTAL_THREADS * sizeof(int));

    numa_free(announce_coord_0, counter_0[NODE_0] * sizeof(AnnounceStruct));
    numa_free(announce_coord_1, counter_0[NODE_1] * sizeof(AnnounceStruct));
    numa_free(announce_coord_2, counter_0[NODE_2] * sizeof(AnnounceStruct));
    numa_free(announce_coord_3, counter_0[NODE_3] * sizeof(AnnounceStruct));

    numa_free(active_numa_zones_0, NUMA_ZONES * sizeof(bool));
    numa_free(active_numa_zones_1, NUMA_ZONES * sizeof(bool));
    numa_free(active_numa_zones_2, NUMA_ZONES * sizeof(bool));
    numa_free(active_numa_zones_3, NUMA_ZONES * sizeof(bool));

    numa_free(delmin_cntr_0, counter_0[NODE_0] * sizeof(DelMinCntr));
    numa_free(delmin_cntr_1, counter_0[NODE_1] * sizeof(DelMinCntr));
    numa_free(delmin_cntr_2, counter_0[NODE_2] * sizeof(DelMinCntr));
    numa_free(delmin_cntr_3, counter_0[NODE_3] * sizeof(DelMinCntr));

    numa_free(counter_0, NUMA_ZONES * sizeof(int));
    numa_free(counter_1, NUMA_ZONES * sizeof(int));
    numa_free(counter_2, NUMA_ZONES * sizeof(int));
    numa_free(counter_3, NUMA_ZONES * sizeof(int));
}

template <class V>
void pq_ns::pq<V>::HeapDeinit(PQ_Heap** heap) {
    HeapList* list = (*heap)->pq_ptr;
    int size = (*heap)->size;
    numa_free((void*)((*heap)->lock), sizeof(atomic_long));
    //de-init all lists (in case we allocated additional)
    while (list) {
        HeapList* temp = list->next;
        numa_free(list->heapList, HEAP_LIST_SIZE * sizeof(PQ_Node));
        numa_free(list, sizeof(HeapList));
        list = temp;
        size -= HEAP_LIST_SIZE;
    }
    numa_free((*heap), sizeof(PQ_Heap));
}

/*         --------------------------------------------         */
/*                                                              */
/*                 HELPER / INS & DEL METHODS                   */
/*                                                              */
/*         --------------------------------------------         */

template <class V>
bool pq_ns::pq<V>::getChildList(HeapList** heapList, int* count, int* c_idx, int size, int heapSize) {
    if (*c_idx >= size) {
        return false;
    }
    while ((*c_idx) >= ((*count) * heapSize)) {
        (*heapList) = (*heapList)->next;
        (*count)++;
        if ((*heapList) == NULL) {
            COUTATOMIC("List is NULL\n");
        }
    }
    *c_idx = *c_idx % heapSize;
    return true;
}

template <class V>
bool pq_ns::pq<V>::getParentList(HeapList** heapList, int* count, int* p_idx, int heapSize) {
    // if the index is out of range, return false 
    if((*p_idx) < 0) {
        return false;
    }
    while ((*p_idx) < ((*count - 1) * heapSize)) {
        (*heapList) = (*heapList)->prev;
        (*count)--;
        if ((*heapList) == NULL) {
            COUTATOMIC("List is NULL-b\n");
        }
    }
    *p_idx = *p_idx % heapSize;
    return true;
}


/*         --------------------------------------------         */
/*                                                              */
/*                      INSERT METHODS                          */
/*                                                              */
/*         --------------------------------------------         */

// template <class V>
// bool pq_ns::pq<V>::hier_insert_local(int key, V value) { // first call by worker whose operation is to insert
//     while (true) {
//         int lock_value = *(t_local_heap->lock);
//         if (lock_value % 2 == 0) {
//             if (__sync_bool_compare_and_swap(t_local_heap->lock, lock_value, lock_value + 1)) {
//                 bool ins_ret = true;
//                 // if (t_local_heap->size == 0 || key < t_local_heap->pq_ptr->heapList[0].key) { // reasons to compare to values at leader level
//                 //     if (t_lead_counters->count >= COUNTER_MAX) {
//                 //         // compare to last_ptr value
//                 //         if (t_largest_in_leader->largest_ptr && key >= t_largest_in_leader->largest_ptr->key) {
//                 //             // insert to worker and return
//                 //             insert_worker(t_local_heap, key, value);
//                 //             t_num_fastpath->count = t_num_fastpath->count + 1;
//                 //         } else {
//                 //             // if (harris_insert(leader_set, t_idx, t_group, key, value)) { //first, insert new element to linked list (okay to have "too many" of some element, but not to have "too few")
//                 //             //     k_t dem_key;
//                 //             //     V dem_val = harris_delete_idx(leader_set, t_idx, t_group, &dem_key);
//                 //             //     if (dem_val != EMPTY) {
//                 //             //         insert_worker(t_local_heap, dem_key, dem_val);
//                 //             //     }
//                 //             // } else {
//                 //             //     ins_ret = false;
//                 //             // }
//                 //             k_t dem_key;
//                 //             V dem_val = harris_insert_and_move(leader_set, t_largest_in_leader, t_idx, t_group, &dem_key, key, value, t_num_traversed);
//                 //             t_num_moves->count = t_num_moves->count + 1;
//                 //             t_num_times_traversed->count = t_num_times_traversed->count + 1;
//                 //             if (dem_val != EMPTY) {
//                 //                 insert_worker(t_local_heap, dem_key, dem_val);
//                 //             } else {
//                 //                 COUTATOMIC("Returned empty...\n");
//                 //             }
//                 //         }
//                 //     } else {
//                 //         if (harris_insert(leader_set, t_largest_in_leader, t_idx, t_group, key, value, t_num_traversed)) {
//                 //             t_num_ins->count = t_num_ins->count + 1;
//                 //             t_num_times_traversed->count = t_num_times_traversed->count + 1;
//                 //             __sync_fetch_and_add(&(t_lead_counters->count), 1);
//                 //         } else {
//                 //             ins_ret = false;
//                 //         }
//                 //     }
//                 // } else {
//                     // insert key at worker (IDEAL CASE)
//                     insert_worker(t_local_heap, key, value);
//                     //t_num_fastpath->count = t_num_fastpath->count + 1;

//                     // perform some helping if needed
//                     // if (t_lead_counters->count < COUNTER_THRESHOLD) {
//                     //     int up_key;
//                     //     std::optional<V> up_val = delete_min_worker(t_local_heap, &up_key);
//                     //     if (up_key != EMPTY) {
//                     //         if (harris_insert(leader_set, t_largest_in_leader, t_idx, t_group, up_key, up_val.value(), t_num_traversed)) {
//                     //             t_help_upsert->count = t_help_upsert->count + 1;
//                     //             t_num_times_traversed->count = t_num_times_traversed->count + 1;
//                     //             __sync_fetch_and_add(&(t_lead_counters->count), 1);
//                     //         } else {
//                     //             repeat_keys[t_group*24 + t_idx] = repeat_keys[t_group*24 + t_idx] + up_key;
//                     //         }
//                     //     }
//                     // }
//                 //}
//                 *(t_local_heap->lock) = *(t_local_heap->lock) + 1;
//                 return ins_ret;
//             }
//         }
//     }
// }

template <class V>
bool pq_ns::pq<V>::hier_insert_local(int key, V value) { // first call by worker whose operation is to insert
    while (true) {
        int lock_value = *(t_local_heap->lock);
        if (lock_value % 2 == 0) {
            if (__sync_bool_compare_and_swap(t_local_heap->lock, lock_value, lock_value + 1)) {
                bool ins_ret = true;
                if (t_local_heap->size == 0 || key < t_local_heap->pq_ptr->heapList[0].key) { // reasons to compare to values at leader level
                    if (t_lead_counters->count >= COUNTER_MAX) {
                        // compare to last_ptr value
                        if (t_largest_in_leader->largest_ptr && key >= t_largest_in_leader->largest_ptr->key) {
                            // insert to worker and return
                            insert_worker(t_local_heap, key, value);
                            t_num_fastpath->count = t_num_fastpath->count + 1;
                        } else {
                            // if (harris_insert(leader_set, t_idx, t_group, key, value)) { //first, insert new element to linked list (okay to have "too many" of some element, but not to have "too few")
                            //     k_t dem_key;
                            //     V dem_val = harris_delete_idx(leader_set, t_idx, t_group, &dem_key);
                            //     if (dem_val != EMPTY) {
                            //         insert_worker(t_local_heap, dem_key, dem_val);
                            //     }
                            // } else {
                            //     ins_ret = false;
                            // }
                            k_t dem_key;
                            V dem_val = harris_insert_and_move(leader_set, t_largest_in_leader, t_idx, t_group, &dem_key, key, value, t_num_traversed);
                            t_num_moves->count = t_num_moves->count + 1;
                            t_num_times_traversed->count = t_num_times_traversed->count + 1;
                            if (dem_val != EMPTY) {
                                insert_worker(t_local_heap, dem_key, dem_val);
                            } else {
                                COUTATOMIC("Returned empty...\n");
                            }
                        }
                    } else {
                        if (harris_insert(leader_set, t_largest_in_leader, t_idx, t_group, key, value, t_num_traversed)) {
                            t_num_ins->count = t_num_ins->count + 1;
                            t_num_times_traversed->count = t_num_times_traversed->count + 1;
                            __sync_fetch_and_add(&(t_lead_counters->count), 1);
                        } else {
                            ins_ret = false;
                        }
                    }
                } else {
                    // insert key at worker (IDEAL CASE)
                    insert_worker(t_local_heap, key, value);
                    t_num_fastpath->count = t_num_fastpath->count + 1;

                    // perform some helping if needed
                    if (t_lead_counters->count < COUNTER_THRESHOLD) {
                        int up_key;
                        std::optional<V> up_val = delete_min_worker(t_local_heap, &up_key);
                        if (up_key != EMPTY) {
                            if (harris_insert(leader_set, t_largest_in_leader, t_idx, t_group, up_key, up_val.value(), t_num_traversed)) {
                                t_help_upsert->count = t_help_upsert->count + 1;
                                t_num_times_traversed->count = t_num_times_traversed->count + 1;
                                __sync_fetch_and_add(&(t_lead_counters->count), 1);
                            } else {
                                repeat_keys[t_group*24 + t_idx] = repeat_keys[t_group*24 + t_idx] + up_key;
                            }
                        }
                    }
                }
                *(t_local_heap->lock) = *(t_local_heap->lock) + 1;
                return ins_ret;
            }
        }
    }
}


template <class V>
void pq_ns::pq<V>::insert_worker(PQ_Heap *Heap, int K, V value) { // check defaults to TRUE
    HeapList* heapListM = Heap->pq_ptr; // heaplist containing m_virt

    if (Heap->size == 0) {
        heapListM->heapList[0].key = K;
        heapListM->heapList[0].value = value;
        (Heap->size)++;
        return;
    }

    HeapList* heapListParent = Heap->pq_ptr; // heaplist containing p_virt (parent of m_virt)
    int countP = 1; // tracks how many lists "in" we are, so we know what to check for in getParentList()
    
    // NOTE: suffix "virt" means it tracks the position as if there were one array, "idx" is the actual index into the respective array
    int m_virt = Heap->size; // index of the next empty position in the heap
    int m_idx = m_virt;
    int p_virt = PARENT(m_virt);
    int p_idx = p_virt;
    
    // how many lists "in" we are
    while (m_idx >= HEAP_LIST_SIZE && heapListM->next) {
        countP++;
        heapListM = heapListM->next;
        heapListParent = heapListParent->next;
        m_idx -= HEAP_LIST_SIZE;
    }

    // check if we need to allocate a new list
    if (m_idx >= HEAP_LIST_SIZE) {
        if (!heapListM->next) {
            heapListM->next = (HeapList*)numa_alloc_local_aligned(sizeof(HeapList));
            heapListM->next->heapList = (PQ_Node*)numa_alloc_local_aligned(HEAP_LIST_SIZE * sizeof(PQ_Node));
            heapListM->next->prev = heapListM;
            heapListM->next->next = nullptr;
        }
        heapListM = heapListM->next;
        heapListParent = heapListParent->next;
        countP++;
        m_idx -= HEAP_LIST_SIZE;
    }

    // currently: countP is the number of lists that exist, heapListParent points to the last list, &p_idx is the virtual index of the parent
    if (!getParentList(&heapListParent, &countP, &p_idx, HEAP_LIST_SIZE)) { // get parent list for "virtual" index p_idx
        heapListM->heapList[m_idx].key = K;
        heapListM->heapList[m_idx].value = value;
        (Heap->size)++;
        return;
    }
    
    while (m_virt > 0 && K < heapListParent->heapList[p_idx].key) { //restore the heap property
        // K has higher priority than its parent, so swap them
        heapListM->heapList[m_idx].key = heapListParent->heapList[p_idx].key;
        heapListM->heapList[m_idx].value = heapListParent->heapList[p_idx].value;

        // reset variables for next loop
        m_virt = p_virt;
        m_idx = p_idx;
        heapListM = heapListParent;
        
        // get the correct parent list and index into it (p_idx) for while loop checks
        p_virt = PARENT(p_virt);
        p_idx = p_virt;
        if (!getParentList(&heapListParent, &countP, &p_idx, HEAP_LIST_SIZE)) {
            break;
        }
    }
    heapListM->heapList[m_idx].key = K;
    heapListM->heapList[m_idx].value = value;
    (Heap->size)++;
    return;
}


/*         --------------------------------------------         */
/*                                                              */
/*                      DELETE METHODS                          */
/*                                                              */
/*         --------------------------------------------         */

template <class V>
int pq_ns::pq<V>::hier_delete(V* val) { // for sssp
    announce_coord[t_idx].status = true;
    try_compete_coordinator();
    int min_priority = announce_coord[t_idx].key;
    *val = announce_coord[t_idx].value;
    return min_priority;
}

template <class V>
int pq_ns::pq<V>::hier_delete() { // for microbenchmarks
    announce_coord[t_idx].status = true;
    try_compete_coordinator();
    return announce_coord[t_idx].key >= 0 ? announce_coord[t_idx].key : 0;
}

//! ccsync for competing for coordinator - look into the paper
// more recently: may be implemented faster

template <class V>
void pq_ns::pq<V>::try_compete_coordinator()  {
    while(true) {
        long lock_value = *t_compete_coord_lock;
        if (lock_value % 2 == 0) {
            // successful CAS -> LEADER role for NUMA zone
            if (__sync_bool_compare_and_swap(t_compete_coord_lock, lock_value, lock_value + 1)) {
                if (!announce_coord[t_idx].status) { // operation has been completed since acquiring lock (unlikely but possible)
                    *t_compete_coord_lock = *t_compete_coord_lock + 1;
                    return;
                }
                try_become_coordinator();
                //Coordinate();
                *t_compete_coord_lock = *t_compete_coord_lock + 1;
                return;
            }
        } else {
            while (*t_compete_coord_lock == lock_value) {
                help_upsert();
                if (!announce_coord[t_idx].status) {
                    return;
                }
            }

            if (!announce_coord[t_idx].status) {
                return;
            }
        }
    }
}

template <class V>
void pq_ns::pq<V>::try_become_coordinator() {
    while(true) {
        long lock_value = (*coord_lock);
        if (lock_value % 2 == 0) {
            // successful CAS => COORDINATOR role
            if (__sync_bool_compare_and_swap(coord_lock, lock_value, lock_value + 1)) {
                Coordinate();
                *coord_lock = *coord_lock + 1;
                return;
            }
        } else {
            while (*coord_lock == lock_value) {
                help_upsert();
            }
        }
    }
}


template <class V>
void pq_ns::pq<V>::Coordinate() {
    int idx;
    int cnt_numops = 0;
    for (idx = 0; idx < t_num_workers; idx++) {
		if(announce_coord[idx].status) { // find active requests from my socket
            delete_min_leader(idx);
			announce_coord[idx].status = false;
            cnt_numops++;
		}
	}
    t_delmin_cntr[t_idx].num = t_delmin_cntr[t_idx].num + 1;
    t_delmin_cntr[t_idx].sum = t_delmin_cntr[t_idx].sum + cnt_numops;
}

template <class V>
void pq_ns::pq<V>::delete_min_leader(int idx) {
    k_t del_key;
    int del_idx, del_zone;
    int counter_tsh = 2; // = 5
    V retval = linden_delete_min(leader_set, &del_key, &del_idx, &del_zone);

    if (del_key != EMPTY) {
        CounterSlot* cntr = get_counters(del_zone, del_idx);
        __sync_add_and_fetch(&(cntr->count), -1);
        announce_coord[idx].key = del_key;
        announce_coord[idx].value = retval;

        if (cntr->count < counter_tsh) { // need to upsert before we can do next del-min
            t_coord_pullup->count = t_coord_pullup->count + 1;
            if (t_group == del_zone && t_idx == del_idx) { // don't have to lock worker
                while (1) {
                    int key_worker;
                    std::optional<V> ret = delete_min_worker(t_local_heap, &key_worker);
                    if (key_worker != EMPTY) {
                        if (cntr->count == 0) {
                            t_largest_in_leader->largest_ptr = NULL;
                        }
                        if (harris_insert(leader_set, t_largest_in_leader, del_idx, del_zone, key_worker, ret.value(), t_num_traversed)) { // if fail, key and value are already present, so remove another from worker and try to insert
                           //t_num_ins->count = t_num_ins->count + 1;
                            t_num_times_traversed->count = t_num_times_traversed->count + 1;
                            __sync_add_and_fetch(&(cntr->count), 1);
                            break;
                        } else {
                            repeat_keys[t_group*24 + t_idx] = repeat_keys[t_group*24 + t_idx] + key_worker;
                        }
                    } else {
                        return;
                    }
                }
            } else {
                // lock worker
                PQ_Heap *worker = get_heap_mapping(del_idx, del_zone);
                LeaderLargest* last_ptr = get_last_ptr(del_idx, del_zone);
                while (true) {
                    int lock_value = *(worker->lock);
                    if (lock_value % 2 == 0) {
                        if (__sync_bool_compare_and_swap(worker->lock, lock_value, lock_value + 1)) {
                            while (1) {
                                int key_worker;
                                std::optional<V> ret = delete_min_worker(worker, &key_worker);
                                if (cntr->count == 0) {
                                    last_ptr->largest_ptr = NULL;
                                }
                                if (key_worker != EMPTY) {
                                    if (harris_insert(leader_set, last_ptr, del_idx, del_zone, key_worker, ret.value(), t_num_traversed)) { // if fail, key and value are already present, so remove another from worker and try to insert
                                        //t_num_ins->count = t_num_ins->count + 1;
                                        t_num_times_traversed->count = t_num_times_traversed->count + 1;
                                        __sync_add_and_fetch(&(cntr->count), 1);
                                        break;
                                    } else {
                                        repeat_keys[t_group*24 + t_idx] = repeat_keys[t_group*24 + t_idx] + key_worker;
                                    }
                                } else {
                                    break;
                                }
                            }
                            *(worker->lock) = *(worker->lock) + 1;
                            return;
                        }
                    } else {
                        while (lock_value == *(worker->lock)) {
                            if (cntr->count >= counter_tsh) { // someone else upserted, we are done
                                return;
                            }
                        }
                    }
                }
            }
        }
    } else {
        announce_coord[idx].key = EMPTY;
        return;
    }
}

template <class V>
void pq_ns::pq<V>::help_upsert() {
    if (t_lead_counters->count < COUNTER_THRESHOLD) {
        int lock_value = *(t_local_heap->lock);
        if (lock_value % 2 == 0) {
            if (__sync_bool_compare_and_swap(t_local_heap->lock, lock_value, lock_value + 1)) {
                // int max_upsert = 3, cnt_upsert = 0; // max to pull up - //! try with 3 too?
                // int max_tries = 5, cnt_tries = 0;
                // while (cnt_upsert < max_upsert && cnt_tries < max_tries && t_lead_counters->count < COUNTER_THRESHOLD) {
                while (1) {
                    //cnt_tries++;
                    int key_worker;
                    std::optional<V> ret = delete_min_worker(t_local_heap, &key_worker);
                    if (key_worker != EMPTY) {
                        if (harris_insert(leader_set, t_largest_in_leader, t_idx, t_group, key_worker, ret.value(), t_num_traversed)) { // if fail, key and value are already present, so remove another from worker and try to insert
                            //cnt_upsert++;
                            t_help_upsert->count = t_help_upsert->count + 1;
                            t_num_times_traversed->count = t_num_times_traversed->count + 1;
                            __sync_add_and_fetch(&(t_lead_counters->count), 1);
                            break;
                        } else {
                            repeat_keys[t_group*24 + t_idx] = repeat_keys[t_group*24 + t_idx] + key_worker;
                        }
                    } else {
                        break;
                    }
                }
                *(t_local_heap->lock) = *(t_local_heap->lock) + 1;
                return;
            }
        }
    }
}

// futureTODO : add in a check for if there's an empty list we should deallocate (?)
template <class V>
std::optional<V> pq_ns::pq<V>::delete_min_worker(PQ_Heap *Heap, int* key) {
    if ((Heap->size) == 0) {
        *key = EMPTY;
        return {};
    }

    HeapList* heapListM = Heap->pq_ptr; // heaplist containing m_virt
    *key = (heapListM->heapList)[0].key;
    int ret_val = (heapListM->heapList)[0].value;
    std::optional<V> retVal = ret_val;
    
    if (Heap->size == 1) {
        (heapListM->heapList)[0].key = NO_VALUE;
        (Heap->size)--;
        return retVal;
    } else if (Heap->size == 2) {
        (heapListM->heapList)[0].key = (heapListM->heapList)[1].key;
        (heapListM->heapList)[0].value = (heapListM->heapList)[1].value;
        (heapListM->heapList)[1].key = NO_VALUE;
        (Heap->size)--;
        return retVal;
    }

    HeapList* heapListLChild = Heap->pq_ptr; // heaplist containing l_virt (left child of m_virt)
    HeapList* heapListRChild = Heap->pq_ptr; // heaplist containing r_virt (right child of m_virt)
    int countL = 1; // tracks how many lists "in" we are, so we know what to check for in getParentList()
    int countR = 1;

    int m_virt = ROOT; // ROOT = 0
    int m_idx = m_virt;
    int l_virt = LEFT_CHILD(m_virt);
    int l_idx = l_virt;
    int r_virt = RIGHT_CHILD(m_virt);
    int r_idx = r_virt;
    
    // get to the last heaplist to get K
    int temp_idx = (Heap->size) - 1;
    HeapList* lastList = Heap->pq_ptr;
    while (lastList->next != nullptr && temp_idx >= HEAP_LIST_SIZE) { // note: since we don't de-init, can't ONLY check "lastList->next != nullptr" bc next ptr may exist even tho the size is smaller
        if (!lastList->next) {
            COUTATOMIC("the next ptr is null!\n");
        }
        lastList = lastList->next;
        temp_idx -= HEAP_LIST_SIZE;
    }

    int K = (lastList->heapList)[temp_idx].key; // last element in the heap
    V val = (lastList->heapList)[temp_idx].value;
    (lastList->heapList)[temp_idx].key = NO_VALUE;
    (Heap->size)--;
    int sizeHeap = Heap->size;

    // get proper lists for child pointers
    if (!getChildList(&heapListLChild, &countL, &l_idx, sizeHeap, HEAP_LIST_SIZE) && !getChildList(&heapListRChild, &countR, &r_idx, sizeHeap, HEAP_LIST_SIZE)) {
        // should never get here I think (bc of checks above)
        (heapListM->heapList)[m_idx].key = K;
        (heapListM->heapList)[m_idx].value = val;
        return retVal;
    }

    // as long as there are smaller (i.e., higher priority) keys down the heap path, continue
    while (( l_virt < sizeHeap && K > (heapListLChild->heapList)[l_idx].key) ||
            (r_virt < sizeHeap && K > (heapListRChild->heapList)[r_idx].key) ) {
                
        if (r_virt < sizeHeap) {
            if ((heapListLChild->heapList)[l_idx].key < (heapListRChild->heapList)[r_idx].key) {
                (heapListM->heapList)[m_idx].key = (heapListLChild->heapList)[l_idx].key;
                (heapListM->heapList)[m_idx].value = (heapListLChild->heapList)[l_idx].value;
                // reset variables for next loop
                m_idx = l_idx;
                m_virt = l_virt;
                heapListM = heapListLChild;
            } else {
                (heapListM->heapList)[m_idx].key = (heapListRChild->heapList)[r_idx].key;
                (heapListM->heapList)[m_idx].value = (heapListRChild->heapList)[r_idx].value;
                // reset variables for next loop
                m_idx = r_idx;
                m_virt = r_virt;
                heapListM = heapListRChild;
            }
        } else {
            (heapListM->heapList)[m_idx].key = (heapListLChild->heapList)[l_idx].key;
            (heapListM->heapList)[m_idx].value = (heapListLChild->heapList)[l_idx].value;
            // reset variables
            m_idx = l_idx;
            m_virt = l_virt;
            heapListM = heapListLChild;
            break;
        }
        
        // reset variables for next loop
        l_virt = LEFT_CHILD(m_virt);
        l_idx = l_virt;
        r_virt = RIGHT_CHILD(m_virt);
        r_idx = r_virt;

        bool retL = getChildList(&heapListLChild, &countL, &l_idx, sizeHeap, HEAP_LIST_SIZE);
        bool retR = getChildList(&heapListRChild, &countR, &r_idx, sizeHeap, HEAP_LIST_SIZE);
        if (!retL && !retR) {
            break;
        }
    }
    (heapListM->heapList)[m_idx].key = K;
    (heapListM->heapList)[m_idx].value = val;
    return retVal;
}