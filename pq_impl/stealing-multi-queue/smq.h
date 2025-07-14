#ifndef GALOIS_STEALINGMULTIQUEUE_H
#define GALOIS_STEALINGMULTIQUEUE_H

#include <atomic>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <array>
#include <chrono>
#include <iterator>
#include <algorithm>
#include <thread>
#include <iostream>

#include "optional.h"
//#include "CacheLineStorage.h"

namespace smq_ns {

  // bool compare(long elem1, long elem2) {
  //   return elem2 < elem1;
  // }

/**
 * Class-helper, consists of a sequential heap and
 * a stealing buffer.
 *
 * @tparam T Type of the elements.
 * @tparam STEAL_NUM Number of elements to steal at once.
 * @tparam D Arity of the heap.
 */
template<typename T, //typename Compare,
         size_t STEAL_NUM,
         size_t D = 4>
class HeapWithStealBuffer {
  typedef size_t index_t;
  // Local priority queue.
  std::vector<T> heap;
  // Other threads steal the whole buffer at once.
  std::array<T, STEAL_NUM> stealBuffer;
  // Represents epoch & stolen flag
  // version mod 2 = 0  -- elements are stolen
  // version mod 2 = 1  -- can steal
  std::atomic<size_t> version;
public:
  // Represents a flag for empty buffer cells.
  static T dummy;
  // Comparator.
  //Compare compare;

  HeapWithStealBuffer(): version(0) {
    for (size_t i = 0; i < STEAL_NUM; i++) {
      stealBuffer[i] = dummy;
    }
  }

  // TRUE if elem2.first < elem1.first
  bool compare(std::pair<long,long> elem1, std::pair<long,long> elem2) {
    return elem2.first < elem1.first;
  }

  long get_keysum() {
    long sum = 0;
    for (int i = 0; i < heap.size(); i++) {
      sum += heap[i].first;
    }
    if (!isBufferStolen()) {
      for (int i = 0; i < STEAL_NUM; i++) {
        if (!isDummy(stealBuffer[i])) {
          sum += stealBuffer[i].first;
        }
      }
    }
    return sum;
  }

  long get_size() {
    long size = heap.size();
    if (!isBufferStolen()) {
      for (int i = 0; i < STEAL_NUM; i++) {
        if (!isDummy(stealBuffer[i])) {
          size++;
        }
      }
    }
    return size;
  }

  //! Checks whether the element is "null".
  static bool isDummy(T const& element) {
    return element == dummy;
  }

  //! Gets current version of the stealing buffer.
  size_t getVersion() {
    return version.load(std::memory_order_acquire);
  }

  //! Checks whether elements in the buffer are stolen.
  size_t isBufferStolen() {
    return getVersion() % 2 == 0;
  }

  //! Fills stealing buffer if the current tasks are stolen.
  void fillBufferIfStolen() {
    if (isBufferStolen()) {
      fillBuffer();
    }
  }

  //! Get min among elements that can be stolen.
  //! Sets a flag to true, if operation failed because of a race.
  T getBufferMin(bool& raceHappened) {
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      return dummy;
    }
    T minVal = stealBuffer[0];
    auto v2 = getVersion();
    if (v1 == v2) {
      return minVal;
    }
    // Somebody has stolen the elements.
    raceHappened = true;
    return dummy;
  }

  //! Returns min element from the buffer, updating the buffer if empty.
  //! Can be called only by the thread-owner.
  T getMinWriter() {
    auto v1 = getVersion();
    if (v1 % 2 != 0) {
      T minVal = stealBuffer[0];
      auto v2 = getVersion();
      if (v1 == v2) {
        return minVal;
      }
    }
    return fillBuffer();
  }

  //! Fills the steal buffer.
  //! Called when the elements from the previous epoch are empty.
  T fillBuffer() {
    if (heap.empty()) return dummy;
    std::array<T, STEAL_NUM> elements;
    elements.fill(dummy);
    for (size_t i = 0; i < STEAL_NUM && !heap.empty(); i++) {
      elements[i] = popLocally();
    }
    stealBuffer = elements;
    version.fetch_add(1, std::memory_order_acq_rel);
    return elements[0];
  }

  //! Tries to steal the elements from the stealing buffer.
  Galois::optional<std::array<T, STEAL_NUM>> trySteal(bool& raceHappened) {
    auto emptyRes = Galois::optional<std::array<T, STEAL_NUM>>();
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      // Already stolen.
      return emptyRes;
    }
    std::array<T, STEAL_NUM> buffer = stealBuffer;
    if (version.compare_exchange_weak(v1, v1 + 1, std::memory_order_acq_rel)) {
      return buffer;
    }
    // Another thread got ahead.
    raceHappened = true;
    return emptyRes;
  }

  //! Retrieves an element from the heap.
  T popLocally() {
    auto res = heap[0];
    heap[0] = heap.back();
    heap.pop_back();
    if (heap.size() > 0) {
      sift_down(0);
    }
    return res;
  }

  //! Extract min from the structure: both the buffer and the heap
  //! are considered. Called from the owner-thread.
  Galois::optional<T> extractMin() {
    if (heap.empty()) {
      // Only check the steal buffer.
      return tryStealLocally();
    }
    bool raceFlag = false;  // useless now
    auto bufferMin = getBufferMin(raceFlag);
    if (!isDummy(bufferMin) && /*(bufferMin < heap[0])*/ compare(heap[0], bufferMin)) {
      auto stolen = tryStealLocally();
      if (stolen.is_initialized()) {
        fillBuffer();
        return stolen;
      }
    }
    auto localMin = popLocally();
    if (isDummy(bufferMin)) fillBuffer();
    return localMin;
  }

  //! Inserts the element into the heap.
  void pushLocally(T const& val) {
    index_t index = heap.size();
    heap.push_back({val});
    sift_up(index);
  }

private:
  //! Tries to steal elements from local buffer.
  //! Return minimum among stolen elements.
  Galois::optional<T> tryStealLocally() {
    bool raceFlag = false;  // useless now
    auto stolen = trySteal(raceFlag);
    if (stolen.is_initialized()) {
      auto elements = stolen.get();
      for (size_t i = 1; i < STEAL_NUM && !isDummy(elements[i]); i++) {
        pushLocally(elements[i]);
      }
      return elements[0];
    }
    return Galois::optional<T>();
  }

  ///////////////////////// HEAP /////////////////////////
  void swap(index_t  i, index_t j) {
    T t = heap[i];
    heap[i] = heap[j];
    heap[j] = t;
  }

  //! Check whether the index of the root passed.
  bool is_root(index_t index) {
    return index == 0;
  }

  //! Check whether the index is not out of bounds.
  bool is_valid_index(index_t index) {
    return index >= 0 && index < heap.size();
  }

  //! Get index of the parent.
  Galois::optional<index_t> get_parent(index_t index) {
    if (!is_root(index) && is_valid_index(index)) {
      return (index - 1) / D;
    }
    return Galois::optional<index_t>();
  }

  //! Get index of the smallest (due `Comparator`) child.
  Galois::optional<index_t> get_smallest_child(index_t index) {
    if (!is_valid_index(D * index + 1)) {
      return Galois::optional<index_t>();
    }
    index_t smallest = D * index + 1;
    for (size_t k = 2; k <= D; k++) {
      index_t k_child = D * index + k;
      if (!is_valid_index(k_child))
        break;
      if (/*heap[k_child] < heap[smallest]*/ compare(heap[smallest], heap[k_child]))
        smallest = k_child;
    }
    return smallest;
  }

  //! Sift down without decrease key info update.
  void sift_down(index_t index) {
    auto smallest_child = get_smallest_child(index);
    while (smallest_child && /*(heap[smallest_child.get()] < heap[index])*/ compare(heap[index], heap[smallest_child.get()])) {
      swap(index, smallest_child.get());
      index = smallest_child.get();
      smallest_child = get_smallest_child(index);
    }
  }

  //! Sift up the element with provided index.
  index_t sift_up(index_t index) {
    Galois::optional<index_t> parent = get_parent(index);

    while (parent && /*(heap[index] < heap[parent.get()])*/ compare(heap[parent.get()], heap[index])) {
      swap(index, parent.get());
      index = parent.get();
      parent = get_parent(index);
    }
    return index;
  }
};

template<typename T,
         //typename Compare,
         size_t STEAL_NUM,
         size_t D>
T HeapWithStealBuffer<T, STEAL_NUM, D>::dummy;

template<typename T,
         //typename Comparer,
         size_t StealProb,
         size_t StealBatchSize,
         bool Concurrent = true>
class StealingMultiQueue {
private:

  typedef HeapWithStealBuffer<T, StealBatchSize, 4> Heap;

  typedef struct alignas(128) heap_container {
    Heap heap;
  } heap_container_t;

  typedef struct alignas(128) vec_container {
    std::vector<T> stealBuf;
  } vec_container_t;

  std::unique_ptr<heap_container_t[]> heaps;
  std::unique_ptr<vec_container_t[]> stealBuffers;
  
  // std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  // std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]> stealBuffers;
  // Comparer compare;
  const size_t nQ;

  //! Thread local random.
  uint32_t random() {
    //static thread_local uint32_t x = std::chrono::system_clock::now().time_since_epoch().count() % 16386 + 1;
    static uint32_t x = std::chrono::system_clock::now().time_since_epoch().count() % 16386 + 1;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  }

  //! Index of a random heap.
  size_t rand_heap() {
    return random() % nQ;
  }


// TRUE if elem2.first < elem1.first
  bool compare(std::pair<long,long> elem1, std::pair<long,long> elem2) {
    return elem2.first < elem1.first;
  }
  //! Tries to steal from a random queue.
  //! Repeats if failed because of a race.
  Galois::optional<T> trySteal() {
    //static thread_local size_t tId = Galois::Runtime::LL::getTID();
    T localMin = heaps[t_tid].heap.getMinWriter();
    bool nextIterNeeded = true;
    while (nextIterNeeded) {
      auto randId = rand_heap();
      if (randId == t_tid) continue;
      nextIterNeeded = false;
      Heap *randH = &heaps[randId].heap;
      auto randMin = randH->getBufferMin(nextIterNeeded);
      if (randH->isDummy(randMin)) {
        // Nothing to steal.
        continue;
      }
      if (Heap::isDummy(localMin) || compare(localMin, randMin)) {
        auto stolen = randH->trySteal(nextIterNeeded);
        if (stolen.is_initialized()) {
          auto &buffer = stealBuffers[t_tid].stealBuf;
          auto elements = stolen.get();
          for (size_t i = 1; i < elements.size() &&
                                 !Heap::isDummy(elements[i]); i++) {
            buffer.push_back(elements[i]);
          }
          std::reverse(buffer.begin(), buffer.end());
          return elements[0];
        }
      }
    }
    return Galois::optional<T>();
  }

public:
  StealingMultiQueue(int num_threads) : nQ(num_threads) {
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Heap::dummy));

    heaps = std::make_unique<heap_container_t[]>(nQ);
    stealBuffers = std::make_unique<vec_container_t[]>(nQ);

    // heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    // stealBuffers = std::make_unique<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]>(nQ);
  }

  inline static thread_local int t_tid;

  typedef T value_type;

  void initThread(int tid);
  unsigned int push(T elem);
  void pop(T* key);

  long del_wrapper(long* key);
  unsigned int ins_wrapper(long key, long value);


  std::string getSizeString();
  bool getValidated();
  long debugKeySum();
  long getSize();


  T get_empty(std::pair<long,long> type) {
      return std::make_pair(-1,-1);
  }

  T get_empty(long type) {
      return -1;
  }



  // void initThread(int tid) {
  //   t_tid = tid;
  // }

  // //! Change the concurrency flag.
  // template<bool _concurrent>
  // struct rethread {
  //   typedef StealingMultiQueue<T, Comparer, StealProb, StealBatchSize, _concurrent> type;
  // };

  // //! Change the type the worklist holds.
  // template<typename _T>
  // struct retype {
  //   typedef StealingMultiQueue<_T, Comparer, StealProb, StealBatchSize, Concurrent> type;
  // };

  // template<typename RangeTy>
  // unsigned int push_initial(const RangeTy &range) {
  //   auto rp = range.local_pair();
  //   return push(rp.first, rp.second);
  // }

  // template<typename Iter>
  // unsigned int push(Iter b, Iter e) {
  //   static thread_local size_t tId = Galois::Runtime::LL::getTID();
  //   if (b == e) return 0;
  //   unsigned int pushedNum = 0;
  //   Heap* heap = &heaps[tId].data;
  //   while (b != e) {
  //     heap->pushLocally(*b++);
  //     pushedNum++;
  //   }
  //   heap->fillBufferIfStolen();
  //   return pushedNum;
  // }
};



//GALOIS_WLCOMPILECHECK(StealingMultiQueue) // todo: what is this ??

}  // namespace WorkList

#endif //GALOIS_STEALINGMULTIQUEUE_H