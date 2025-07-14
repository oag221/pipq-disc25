#pragma once

#include "sssp_smq.h"

inline long smq_remove(smq_t *pq, unsigned long* key) {
    std::pair<long,long> elem = {-1, -1};
    pq->pop(&elem);
    *key = elem.first;
    return elem.second;
}

inline void smq_insert(smq_t *pq, long key, long val) {
    pq->push(std::make_pair(key, val));
}