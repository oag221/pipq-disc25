#pragma once

// Start Linden Common

#include <assert.h>
#include <errno.h>
#include <gsl/gsl_rng.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sched.h>
#include <sys/syscall.h>
#include <time.h>
#include <string>

/* keir fraser's garbage collection */
#include "ptst.h"

#include "utils.h"
#include "skiplist.h"
//#include "thread_data.h"

typedef unsigned long val_t;

#define DCL_ALIGN __attribute__((aligned(2 * CACHE_LINE_SIZE)))
#define CACHELINE __attribute__((aligned(1 * CACHE_LINE_SIZE)))

#define ATPAGESIZE __attribute__((aligned(PAGESIZE)))

#define SQR(x) (x) * (x)

#define linden_max(a, b)                                                       \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })

#define linden_min(a, b)                                                       \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a < _b ? _a : _b;                                                         \
  })

typedef struct thread_args_s {
  pthread_t thread;
  int id;
  gsl_rng *rng;
  int measure;
  int cycles;
  char pad[128];
} thread_args_t;

#define E(c)                                                                   \
  do {                                                                         \
    int _c = (c);                                                              \
    if (_c < 0) {                                                              \
      fprintf(stderr, "E: %s: %d: %s\n", __FILE__, __LINE__, #c);              \
    }                                                                          \
  } while (0)

#define E_en(c)                                                                \
  do {                                                                         \
    int _c = (c);                                                              \
    if (_c != 0) {                                                             \
      fprintf(stderr, strerror(_c));                                           \
    }                                                                          \
  } while (0)

#define E_NULL(c)                                                              \
  do {                                                                         \
    if ((c) == NULL) {                                                         \
      perror("E_NULL");                                                        \
    }                                                                          \
  } while (0)

/* accurate time measurements on late recent cpus */
static inline uint64_t __attribute__((always_inline)) read_tsc_p() {
  uint64_t tsc;
  __asm__ __volatile__("rdtscp\n"
                       "shl $32, %%rdx\n"
                       "or %%rdx, %%rax"
                       : "=a"(tsc)
                       :
                       : "%rcx", "%rdx");
  return tsc;
}

#define IMB() __asm__ __volatile__("mfence" ::: "memory")
#define IRMB() __asm__ __volatile__("lfence" ::: "memory")
#define IWMB() __asm__ __volatile__("sfence" ::: "memory")

extern pid_t gettid(void);
#ifdef LINDEN_PIN
extern void pin(pid_t t, int cpu);
#endif

extern void gettime(struct timespec *t);
extern struct timespec timediff(struct timespec, struct timespec);

// End Linden Common

typedef int pkey_t;
typedef unsigned long pval_t;

#define KEY_NULL 0
#define NUM_LEVELS 32
/* Internal key values with special meanings. */
#define SENTINEL_KEYMIN (0)  /* Key value of first dummy node. */
//#define SENTINEL_KEYMAX (~1UL) /* Key value of last dummy node.  */
#define SENTINEL_KEYMAX (INT_MAX)

struct node_t {
  pkey_t k;
  int level;
  int inserting; // char pad2[4];
  pval_t v;
  struct node_t *next[1];
};

struct pq_t {
  int max_offset; // first three (ints) unused, but not sure if it will affect padding so Im leaving for now
  int max_level;
  int nthreads;
  node_t *head;
  node_t *tail;
  char pad[128];
};

#define get_marked_ref(_p) ((void *)(((uintptr_t)(_p)) | 1))
#define get_unmarked_ref(_p) ((void *)(((uintptr_t)(_p)) & ~1))
#define is_marked_ref(_p) (((uintptr_t)(_p)) & 1)

/* Interface */
namespace linden_ns {

  class linden {
    public:
      pq_t* pq;
      /* thread state. */
      //__thread ptst_t *ptst; //todo: is it an issue to remove __thread ??
      //inline static thread_local ptst_t *ptst;

      const int MAX_OFFSET;
      const int MAX_LEVEL;
      const int TOTAL_THREADS;
      linden(int max_offset, int max_level, int nthreads) :
          MAX_OFFSET(max_offset),
          MAX_LEVEL(max_level),
          TOTAL_THREADS(nthreads) {}

      pq_t *pq_init();
      void pq_destroy();
      int insert_linden(pkey_t k, pval_t v);
      pval_t deletemin_key(pkey_t *key);
      void sequential_length();
      int empty();
      long long getKeySum();
      long long debugKeySum();
      long long getSize();
      std::string getSizeString();
      bool getValidated();

      node_t* locate_preds(pkey_t k, node_t **preds, node_t **succs);
      void restructure();
      node_t* alloc_node(pq_t *);
      void free_node(node_t *n);
  };
}