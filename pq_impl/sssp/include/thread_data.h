#pragma once

#include <exception>
#include <pthread.h>
#include <string>
#include <stdexcept>

typedef unsigned long val_t;

// #include "sv_lin.h"
// #include "sv_teams.h"
#include "numa_pq_sssp.h"
#include "numa_pq_4leaders_sssp.h"
#include "sssp_smq.h"

struct barrier_t {
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
};

struct pq_t;

/// An enum of the different data structures we support, so that we can support
/// more options without needing so many command line arguments.
enum DataStructure {
  UNKNOWN,
  SPRAY,
  LOTAN,
  LINDEN,
  // SV_LIN,
  // SV_TEAMS,
  NUMA_PQ,
  NUMA_PQ_4,
  SMQ
};

// NB: in test.cc, there are if/else chains that don't afford clean errors when
// a new ds is added.  They have to do with "alternate" mode?!?
inline const char *to_string(DataStructure ds) {
  switch (ds) {
  case SPRAY:
    return "SprayList";
  case LOTAN:
    return "Lotan-Shavit";
  case LINDEN:
    return "Linden";
  // case SV_LIN:
  //   return "SkipVector-Linearizable";
  // case SV_TEAMS:
  //   return "SkipVector-Teams";
  case NUMA_PQ:
    return "Numa-PQ";
  case NUMA_PQ_4:
    return "Numa-PQ-4";
  case SMQ:
    return "SMQ";
  default:
    std::string msg = "Invalid data structure: ";
    msg += std::to_string(ds);
    throw std::invalid_argument(msg);
  }
  return "";
}

struct ALIGNED(64) thread_data_t {
  val_t first;
  long range;
  int update;
  int unit_tx;
  int alternate;
  int randomized;
  DataStructure ds;
  int es;
  int effective;
  int first_remove;
  unsigned long nb_collisions;
  unsigned long nb_clean;
  unsigned long nb_insertions;
  unsigned long nb_added;
  unsigned long nb_removals;
  unsigned long nb_removed;
  unsigned long nb_dead_nodes;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned long nb_aborts;
  unsigned long nb_aborts_locked_read;
  unsigned long nb_aborts_locked_write;
  unsigned long nb_aborts_validate_read;
  unsigned long nb_aborts_validate_write;
  unsigned long nb_aborts_validate_commit;
  unsigned long nb_aborts_invalid_memory;
  unsigned long nb_aborts_double_write;
  unsigned long max_retries;
  unsigned int nb_threads;
  unsigned int seed;
  unsigned int seed2;
  sl_intset_t *set;
  // sv_lin_t *sv_lin;
  // sv_teams_t *sv_teams;
  numa_pq_t *numa_pq_ds;
  numa_pq_4_t *numa_pq_4_ds;
  smq_t *smq_ds;
  barrier_t *barrier;
  unsigned long failures_because_contention;
  int id;

  /* LINDEN */
  pq_t *linden_set;
};
