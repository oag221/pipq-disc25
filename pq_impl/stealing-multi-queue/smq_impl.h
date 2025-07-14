#include "smq.h"

#include <string>

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
void smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::initThread(int tid) {
    t_tid = tid;
}

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
long smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::del_wrapper(long* key) {
    std::pair<long,long> ret = {-1, -1};
    pop(&ret);
    *key = ret.first;
    return ret.second;
}

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
void smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::pop(T* elem) {
    auto& buffer = stealBuffers[t_tid].stealBuf;
    if (!buffer.empty()) {
        auto val = buffer.back();
        buffer.pop_back();
        heaps[t_tid].heap.fillBufferIfStolen();
        *elem = val;
        return;
    }
    T emptyResult = get_empty(*elem);
    // rand == 0 -- try to steal
    // otherwise, pop locally
    if (nQ > 1 && random() % StealProb == 0) {
        Galois::optional<T> stolen = trySteal();
        if (stolen.is_initialized()) {
            *elem = stolen.get();
            return;
        } 
    }
    auto minVal = heaps[t_tid].heap.extractMin();
    if (minVal.is_initialized()) {
        *elem = minVal.get();
        return;
    }

    // Our heap is empty.
    if (nQ == 1) {
        *elem = emptyResult;
        return;
    }
    Galois::optional<T> res = trySteal();
    if (res.is_initialized()) {
        *elem = res.get();
        return;
    }
    *elem = emptyResult;
    return;
}

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
unsigned int smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::ins_wrapper(long key, long value) {
   return push(std::make_pair(key, value));
}

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
unsigned int smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::push(T elem) {
    Heap* heap = &heaps[t_tid].heap;
    heap->pushLocally(elem);
    heap->fillBufferIfStolen();
    return 1;
  }

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
long smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::debugKeySum() {
    long sum = 0;
    for (int i = 0; i < nQ; i++) {
        Heap *randH = &heaps[i].heap;
        sum += randH->get_keysum();
    }
    return sum;
}

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
bool smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::getValidated() {
    return true;
}

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
long smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::getSize() {
    long size = 0;
    for (int i = 0; i < nQ; i++) {
        Heap *randH = &heaps[i].heap;
        size += randH->get_size();
    }
    return size;
}


template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent>
std::string smq_ns::StealingMultiQueue<T, StealProb, StealBatchSize, Concurrent>::getSizeString() {
    return std::to_string(getSize());
}
