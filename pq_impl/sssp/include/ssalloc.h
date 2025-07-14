// [mfs] ssalloc is a pooled allocator.  It was only used by the SprayList and
//       the Lotan-Shavit priority queue... it was **not** used by Linden.  When
//       this pooled allocator was used (which avoids calls to malloc/free,
//       instead using a local pool), SprayList and Lotan-Shavit were more than
//       2x faster than when they did not use the pool.  Using jemalloc and
//       disabling the pool allocator gives roughly the same performance to
//       SprayList and Lotan-Shavit, is more fair, and avoids the complexity of
//       the ssalloc infrastructure, so we have disabled ssalloc.

#pragma once

#include <cstdlib>

#define SSALLOC_NUM_ALLOCATORS 2

#define SSALLOC_SIZE (1024 * 1024 * 1024)

// [mfs] Toggle this to see the difference
// #define USE_SSALLOC

#if USE_SSALLOC

void ssalloc_set(void *mem);
void ssalloc_init();
void ssalloc_align();
void ssalloc_align_alloc(unsigned int allocator);
void ssalloc_offset(size_t size);
void *ssalloc_alloc(unsigned int allocator, size_t size);
void ssfree_alloc(unsigned int allocator, void *ptr);
void *ssalloc(size_t size);
void ssfree(void *ptr);

#else
inline void ssalloc_init(){};
inline void *ssalloc(size_t size) { return aligned_alloc(64, size); }
inline void ssfree(void *ptr) { free(ptr); }
inline void ssfree_alloc(unsigned int, void *ptr) { free(ptr); }
inline void *ssalloc_alloc(unsigned int, size_t size) {
  return aligned_alloc(64, size);
}
inline void ssalloc_align_alloc(unsigned int) {}
#endif