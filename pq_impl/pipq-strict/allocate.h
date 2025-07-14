#ifndef ALLOCATE_H
#define ALLOCATE_H

#include <unistd.h>
#include <sched.h> 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <numa.h>
#include "numa_pq.h"

using namespace std;
using namespace pq_ns;

#define NUMA_ZONES 4
#define NUMA_ZONE_THREADS 48
#define CUTOFF 96

#define lock(m) m.lock();
#define unlock(m) m.unlock();

//#define NEWCELL() malloc(sizeof(llNode));

struct option_ {
  const char *name;
  int         has_arg;
  int        *flag;
  int         val;
};

extern char *optarg;
static __thread volatile long __twork;

typedef unsigned int uint32_t;
typedef signed int int32_t;

static uint32_t __ncores = 0;
//static __thread int32_t __prefered_core = -1;

uint32_t synchGetNCores(void) {
    if (__ncores == 0)
        __ncores = sysconf(_SC_NPROCESSORS_ONLN);
    return __ncores;
}

extern int getopt_long(int nargc, char * const *nargv, const char *options,
const struct option_ *long_options, int *idx);

uint32_t synchGetNCores(void);
uint32_t synchPreferedCoreOfThread(uint32_t pid);

int synchThreadPin(int32_t cpu_id);

void *numa_alloc_local_aligned(size_t size);
void *numa_alloc_aligned(size_t size, int group);

void initialize();
//void Announce_allocation(AnnounceStruct **anounce, int size, int group);

bool GetLock(volatile long *lock);
void ReleaseLock(volatile long *lock);

#endif /* ALLOCATE_H */