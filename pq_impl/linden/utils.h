// some utility functions

#pragma once

#include <emmintrin.h>
#include <errno.h>
#include <inttypes.h>
#include <numa.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <xmmintrin.h>

#include "atomic_ops_if.h"
#include "getticks.h"
#include "random.h"
#include "ssalloc.h"

#define DO_ALIGN
/* #define DO_PAD */

#if defined(DO_ALIGN)
#define ALIGNED(N) __attribute__((aligned(N)))
#else
#define ALIGNED(N)
#endif

#ifdef __sparc__
#define PAUSE asm volatile("rd    %%ccr, %%g0\n\t" ::: "memory")

#elif defined(__tile__)
#define PAUSE cycle_relax()
#else
#define PAUSE _mm_pause()
#endif
static inline void pause_rep(uint32_t num_reps) {
  uint32_t i;
  for (i = 0; i < num_reps; i++) {
    PAUSE;
    /* PAUSE; */
    /* asm volatile ("NOP"); */
  }
}

static inline void nop_rep(uint32_t num_reps) {
  uint32_t i;
  for (i = 0; i < num_reps; i++) {
    asm volatile("NOP");
  }
}

// machine dependent parameters
#define NUMBER_OF_SOCKETS 1
#define CORES_PER_SOCKET 8
#define CACHE_LINE_SIZE 64
#define NOP_DURATION 1
static uint8_t __attribute__((unused))
the_cores[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
               32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
static uint8_t __attribute__((unused)) the_sockets[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* PLATFORM specific
 * -------------------------------------------------------------------- */
#define PREFETCHW(x)

// debugging functions
#ifndef NDEBUG
#define DPRINT(args...) fprintf(stderr, args);
#define DDPRINT(fmt, args...)                                                  \
  printf("%s:%s:%d: " fmt, __FILE__, __FUNCTION__, __LINE__, args)
#else
#define DPRINT(...)
#define DDPRINT(fmt, ...)
#endif

static inline int get_cluster(int thread_id) {
  return thread_id / CORES_PER_SOCKET;
}

static inline double wtime(void) {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (double)t.tv_sec + ((double)t.tv_usec) / 1000000.0;
}

static inline void set_cpu(int cpu) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);

  numa_set_preferred(get_cluster(cpu));

  pthread_t thread = pthread_self();
  if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &mask) != 0) {
    fprintf(stderr, "Error setting thread affinity\n");
  }
}

static inline void cdelay(ticks cycles) {
  ticks __ts_end = getticks() + (ticks)cycles;
  while (getticks() < __ts_end)
    ;
}

static inline void cpause(ticks cycles) {
#if defined(XEON)
  cycles >>= 3;
  ticks i;
  for (i = 0; i < cycles; i++) {
    _mm_pause();
  }
#else
  ticks i;
  for (i = 0; i < cycles; i++) {
    __asm__ __volatile__("nop");
  }
#endif
}

static inline void udelay(unsigned int micros) {
  double __ts_end = wtime() + ((double)micros / 1000000);
  while (wtime() < __ts_end)
    ;
}

// getticks needs to have a correction because the call itself takes a
// significant number of cycles and skewes the measurement
ticks getticks_correction_calc();

static inline ticks get_noop_duration() {
#define NOOP_CALC_REPS 1000000
  ticks noop_dur = 0;
  uint32_t i;
  ticks corr = getticks_correction_calc();
  ticks start;
  ticks end;
  start = getticks();
  for (i = 0; i < NOOP_CALC_REPS; i++) {
    __asm__ __volatile__("nop");
  }
  end = getticks();
  noop_dur = (ticks)((end - start - corr) / (double)NOOP_CALC_REPS);
  return noop_dur;
}

/// Round up to next higher power of 2 (return x if it's already a power
/// of 2) for 32-bit numbers
static inline uint32_t pow2roundup(uint32_t x) {
  if (x == 0)
    return 1;
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}
