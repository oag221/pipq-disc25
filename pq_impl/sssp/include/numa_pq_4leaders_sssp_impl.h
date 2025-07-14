#pragma once

#include "numa_pq_4leaders_sssp.h"

inline void numa_pq_4_remove(numa_pq_4_t *pq, long unsigned *key, long unsigned *val) {
    *key = pq->hier_delete(val);
}

inline void numa_pq_4_insert(numa_pq_4_t *pq, long key, long val) {
    pq->hier_insert_local(key, val);
}