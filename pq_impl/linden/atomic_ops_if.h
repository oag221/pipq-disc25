/*
 *  File: atomic_ops.h
 *  Author: Tudor David
 *
 *  Created on: December 06, 2012
 *
 *  Description:
 *      Cross-platform atomic operations
 */

#pragma once

#include <inttypes.h>

/*
 *  x86 code
 */

#include <xmmintrin.h>

// Swap pointers
static inline void *swap_pointer(volatile void *ptr, void *x) {
#ifdef __i386__
  __asm__ __volatile__("xchgl %0,%1"
                       : "=r"((unsigned)x)
                       : "m"(*(volatile unsigned *)ptr), "0"(x)
                       : "memory");

  return x;
#elif defined(__x86_64__)
  __asm__ __volatile__("xchgq %0,%1"
                       : "=r"((unsigned long long)x)
                       : "m"(*(volatile long long *)ptr),
                         "0"((unsigned long long)x)
                       : "memory");

  return x;
#endif
}

// Swap uint64_t
static inline uint64_t swap_uint64(volatile uint64_t *target, uint64_t x) {
  __asm__ __volatile__("xchgq %0,%1"
                       : "=r"((uint64_t)x)
                       : "m"(*(volatile uint64_t *)target), "0"((uint64_t)x)
                       : "memory");

  return x;
}

// Swap uint32_t
static inline uint32_t swap_uint32(volatile uint32_t *target, uint32_t x) {
  __asm__ __volatile__("xchgl %0,%1"
                       : "=r"((uint32_t)x)
                       : "m"(*(volatile uint32_t *)target), "0"((uint32_t)x)
                       : "memory");

  return x;
}

// Swap uint16_t
static inline uint16_t swap_uint16(volatile uint16_t *target, uint16_t x) {
  __asm__ __volatile__("xchgw %0,%1"
                       : "=r"((uint16_t)x)
                       : "m"(*(volatile uint16_t *)target), "0"((uint16_t)x)
                       : "memory");

  return x;
}

// Swap uint8_t
static inline uint8_t swap_uint8(volatile uint8_t *target, uint8_t x) {
  __asm__ __volatile__("xchgb %0,%1"
                       : "=r"((uint8_t)x)
                       : "m"(*(volatile uint8_t *)target), "0"((uint8_t)x)
                       : "memory");

  return x;
}

// test-and-set uint8_t
static inline uint8_t tas_uint8(volatile uint8_t *addr) {
  uint8_t oldval;
  __asm__ __volatile__("xchgb %0,%1"
                       : "=q"(oldval), "=m"(*addr)
                       : "0"((unsigned char)0xff), "m"(*addr)
                       : "memory");
  return (uint8_t)oldval;
}

// atomic operations interface
// Compare-and-swap
#define CAS_PTR(a, b, c) __sync_val_compare_and_swap(a, b, c)
#define CAS_U8(a, b, c) __sync_val_compare_and_swap(a, b, c)
#define CAS_U16(a, b, c) __sync_val_compare_and_swap(a, b, c)
#define CAS_U32(a, b, c) __sync_val_compare_and_swap(a, b, c)
#define CAS_U64(a, b, c) __sync_val_compare_and_swap(a, b, c)
// Swap
#define SWAP_PTR(a, b) swap_pointer(a, b)
#define SWAP_U8(a, b) swap_uint8(a, b)
#define SWAP_U16(a, b) swap_uint16(a, b)
#define SWAP_U32(a, b) swap_uint32(a, b)
#define SWAP_U64(a, b) swap_uint64(a, b)
// Fetch-and-increment
#define FAI_U8(a) __sync_fetch_and_add(a, 1)
#define FAI_U16(a) __sync_fetch_and_add(a, 1)
#define FAI_U32(a) __sync_fetch_and_add(a, 1)
#define FAI_U64(a) __sync_fetch_and_add(a, 1)
// Fetch-and-decrement
#define FAD_U8(a) __sync_fetch_and_sub(a, 1)
#define FAD_U16(a) __sync_fetch_and_sub(a, 1)
#define FAD_U32(a) __sync_fetch_and_sub(a, 1)
#define FAD_U64(a) __sync_fetch_and_sub(a, 1)
// Increment-and-fetch
#define IAF_U8(a) __sync_add_and_fetch(a, 1)
#define IAF_U16(a) __sync_add_and_fetch(a, 1)
#define IAF_U32(a) __sync_add_and_fetch(a, 1)
#define IAF_U64(a) __sync_add_and_fetch(a, 1)
// Decrement-and-fetch
#define DAF_U8(a) __sync_sub_and_fetch(a, 1)
#define DAF_U16(a) __sync_sub_and_fetch(a, 1)
#define DAF_U32(a) __sync_sub_and_fetch(a, 1)
#define DAF_U64(a) __sync_sub_and_fetch(a, 1)
// Test-and-set
#define TAS_U8(a) tas_uint8(a)
// Memory barrier
/* #define MEM_BARRIER __sync_synchronize() */
#define MEM_BARRIER // nop on the opteron for these benchmarks
// Relax CPU
//#define PAUSE _mm_pause()

/* start --generic code */

#define CAS_U64_bool(addr, old, new) (old == CAS_U64(addr, old, new))

/* static inline uint8_t */
/* CAS_U64_bool(volatile AO_t* addr, AO_t old, AO_t new) */
/* { */
/*   return (old == CAS_U64(addr, old, new)); */
/* } */

/* #define ATOMIC_CAS_MB(a, e, v)          (AO_compare_and_swap_full((volatile
 * AO_t *)(a), (AO_t)(e), (AO_t)(v))) */
/* #define ATOMIC_FETCH_AND_INC_FULL(a)    (AO_fetch_and_add1_full((volatile
 * AO_t *)(a))) */

#define ATOMIC_CAS_MB(a, e, v)                                                 \
  CAS_U64_bool((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v))
#define ATOMIC_FETCH_AND_INC_FULL(a) FAI_U64((volatile AO_t *)(a))

/* end -- generic code */