#pragma once

#include <pthread.h>
#include <sched.h>

/*
 * I. Compare-and-swap.
 */

/*
 * This is a strong barrier! Reads cannot be delayed beyond a later store.
 * Reads cannot be hoisted beyond a LOCK prefix. Stores always in-order.
 */
#define CAS(_a, _o, _n)                                                        \
  ({                                                                           \
    __typeof__(_o) __o = _o;                                                   \
    __asm__ __volatile__("lock cmpxchg %3,%1"                                  \
                         : "=a"(__o), "=m"(*(volatile unsigned int *)(_a))     \
                         : "0"(__o), "r"(_n));                                 \
    __o;                                                                       \
  })

#define FAS(_a, _n)                                                            \
  ({                                                                           \
    __typeof__(_n) __o;                                                        \
    __asm__ __volatile__("lock xchg %0,%1"                                     \
                         : "=r"(__o), "=m"(*(volatile unsigned int *)(_a))     \
                         : "0"(_n));                                           \
    __o;                                                                       \
  })

#define CAS64(_a, _o, _n)                                                      \
  ({                                                                           \
    __typeof__(_o) __o = _o;                                                   \
    __asm__ __volatile__("movl %3, %%ecx;"                                     \
                         "movl %4, %%ebx;"                                     \
                         "lock cmpxchg8b %1"                                   \
                         : "=A"(__o),                                          \
                           "=m"(*(volatile unsigned long long *)(_a))          \
                         : "0"(__o), "m"(_n >> 32), "m"(_n)                    \
                         : "ebx", "ecx");                                      \
    __o;                                                                       \
  })

/* Update Integer location, return Old value. */
#define CASIO CAS
#define FASIO FAS
/* Update Pointer location, return Old value. */
#define CASPO CAS
#define FASPO FAS
/* Update 32/64-bit location, return Old value. */
#define CAS32O CAS
#define CAS64O CAS64

/*
 * II. Memory barriers.
 *  WMB(): All preceding write operations must commit before any later writes.
 *  RMB(): All preceding read operations must commit before any later reads.
 *  MB():  All preceding memory accesses must commit before any later accesses.
 *
 *  If the compiler does not observe these barriers (but any sane compiler
 *  will!), then VOLATILE should be defined as 'volatile'.
 */
#define MB() __sync_synchronize()
#define WMB() __asm__ __volatile__("" : : : "memory")
#define RMB() MB()
#define VOLATILE /*volatile*/

/* On Intel, CAS is a strong barrier, but not a compile barrier. */
#define RMB_NEAR_CAS() WMB()
#define WMB_NEAR_CAS() WMB()
#define MB_NEAR_CAS() WMB()

/*
 * III. Cycle counter access.
 */
typedef unsigned long long tick_t;

static inline tick_t __attribute__((always_inline)) RDTICK() {
  tick_t __t;
  __asm__ __volatile__("rdtsc\n"
                       "shl $32,%%rdx\n"
                       "or %%rdx,%%rax"
                       : "=a"(__t)
                       :
                       : "%rcx", "%rdx");
  return __t;
}

/*
 * IV. Types.
 */
typedef unsigned char _u8;
typedef unsigned short _u16;
typedef unsigned int _u32;
typedef unsigned long long _u64;
