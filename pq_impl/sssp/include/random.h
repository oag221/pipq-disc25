#pragma once

#include <malloc.h>

#include "getticks.h"
#include "ssalloc.h"

extern __thread unsigned long *seeds;

#define my_random xorshf96

static inline unsigned long *seed_rand() {
  unsigned long *seeds;
  /* seeds = (unsigned long*) malloc(3 * sizeof(unsigned long)); */
  seeds = (unsigned long *)ssalloc(3 * sizeof(unsigned long));
  /* seeds = (unsigned long*) memalign(64, 64); */
  seeds[0] = getticks() % 123456789;
  seeds[1] = getticks() % 362436069;
  seeds[2] = getticks() % 521288629;
  return seeds;
}

// Marsaglia's xorshf generator
// period 2^96-1
static inline unsigned long xorshf96(unsigned long *x, unsigned long *y,
                                     unsigned long *z) {
  unsigned long t;
  (*x) ^= (*x) << 16;
  (*x) ^= (*x) >> 5;
  (*x) ^= (*x) << 1;

  t = *x;
  (*x) = *y;
  (*y) = *z;
  (*z) = t ^ (*x) ^ (*y);

  return *z;
}

static inline long rand_range(long r) {
  long v = xorshf96(seeds, seeds + 1, seeds + 2) % r;
  v++;
  return v;
}

/* Re-entrant version of rand_range(r) */
static inline long rand_range_re(unsigned int *, long r) {
  long v = xorshf96(seeds, seeds + 1, seeds + 2) % r;
  v++;
  return v;
}

/******************************************************************************
 * A really simple random-number generator. Crappy linear congruential
 * taken from glibc, but has at least a 2^32 period.
 */

typedef unsigned long rand_t;
#define rand_init(_ptst) ((_ptst)->rand = RDTICK())
#define rand_next(_ptst) ((_ptst)->rand = ((_ptst)->rand * 1103515245) + 12345)
