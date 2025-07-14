

//#define _GNU_SOURCE

#include <unistd.h>
#include <sched.h> 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <numa.h>

#include "allocate.h"
#include "numa_pq.h"

void *numa_alloc_local_aligned(size_t size) { //alligned allocation for the NUMA protocol
    void *p     = numa_alloc_local(size); // note: this is a built in function which allocates "size" bytes on the local node
    long plong  = (long)p;
    plong       += ALIGN_SIZE;
    plong       &= ~(ALIGN_SIZE - 1);
    p           = (void *)plong;
    return p;
}

void *numa_alloc_aligned(size_t size, int group) { //alligned allocation for the NUMA protocol
    void *p     = numa_alloc_onnode(size, group); // note: this is a built in function which allocates "size" bytes on the local node
    long plong  = (long)p;
    plong       += ALIGN_SIZE;
    plong       &= ~(ALIGN_SIZE - 1);
    p           = (void *)plong;
    return p;
}


// void Announce_allocation(pq_ns::AnnounceStruct **announce, int size, int group) {
//     int i, j;
//     (*announce) = (pq_ns::AnnounceStruct *)numa_alloc_aligned(sizeof(pq_ns::AnnounceStruct) * size, group);
//     for (i = 0; i < size; i++) {
//         (*announce)[i].status  = false;
//         (*announce)[i].value = NO_VALUE;
//         (*announce)[i].type = EMPTY;
//         (*announce)[i].detected = 0;
//     } 
// }



inline bool GetLock(volatile long *lock) {
    long lock_value = *lock;
    if (lock_value % 2 == 0) {
        if (__sync_bool_compare_and_swap(lock, lock_value, lock_value + 1)) {
            return true;
        }
    }
    return false;
}

inline void ReleaseLock(volatile long *lock) { (*lock) ++; }

// void PrintHeap (PQ_Heap *Heap) {
//     size_t i;
//     HeapList* list = Heap->pq_ptr;
//     for (i = 0; i < Heap->size; i++)
//     {
//         printf("%d ", list->heapList[i].key);
//         // TODO: need to adjust for many lists
//     }
//     printf("\n");
// }

/*
NodePool *PoolInit (NodePool *pool, int size) {
    pool = numa_alloc_local_aligned(sizeof(NodePool) + ALIGN_SIZE);

    pool->NodeLocks  = numa_alloc_local(size * sizeof(long));
    pool->Pool       = numa_alloc_local(size * sizeof(llNode*));
    pool->Availability = numa_alloc_local(size * sizeof(bool));
    pool->NextNode   = 0;
    pool->FirstEmpty = 0;
    pool->size = size;

    int i;
    for(i = 0; i < size; i++) {
        llNode *node = numa_alloc_local_aligned(sizeof(llNode) + ALIGN_SIZE);
        pool->Pool[i] = node;
        pool->Availability[i] = true;
    }

    return pool;
}

llNode *GetNode(NodePool *pool) {
    int curr = pool->NextNode;
    int size = pool->size;
    while(true) {
        if(GetLock(&pool->NodeLocks[curr])) {
            if(pool->Availability[curr] == false) {
                ReleaseLock(&pool->NodeLocks[curr]);
            }
            else {
                __atomic_fetch_add(&pool->NextNode, 1, __ATOMIC_SEQ_CST);
                pool->Availability[curr] = false;
                ReleaseLock(&pool->NodeLocks[curr]);
                return pool->Pool[curr];
            }
        }
        curr = (curr + 1) % size;
    }
}*/

/*
List *ListInit (List *list) {
    list       = malloc(sizeof(List));
    list->head = numa_alloc_local_aligned(sizeof(pq_ns::llNode) + ALIGN_SIZE);
    list->tail = numa_alloc_local_aligned(sizeof(pq_ns::llNode) + ALIGN_SIZE);
    list->size = 0;

    list->head->key  = EMPTY;
    list->tail->key  = EMPTY;

    list->head->next = list->tail;
    list->tail->prev = list->head;

    list->head->prev = NULL;
    list->tail->next = NULL;

    return list;
}

void PrintList (llNode *head) {
    llNode *curr = head;
    while (curr) {
        printf("%d ", curr->key);
        curr = curr->next;
    }
    printf("\n");
}*/