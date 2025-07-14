#pragma once

#include "../../common/entry.h"
#include "../../common/machine_defines.h"
#include "../../hp/hp_manager.h"
#include "../../skipvector-pq/skipvector_pq_teams.h"
#include "../../vector/multivector_asr.h"

constexpr slkey_t EMPTY_T = static_cast<slkey_t>(-1);
using ENTRY_T = entry<slkey_t, val_t, EMPTY_T>;
constexpr int MAX_TEAMS_T = MAX_THREADS / THREADS_PER_CORE;

#if 0
// Autotune result, Teams Implementatation, Mixed workload, Inserts use RDTSCP,
// 16 threads, 1m elements
constexpr int DATA_SIZE_T = 4096;
constexpr int IDX_SIZE_T = 256;
constexpr int MAX_LAYERS_T = 8;
#endif

// Autotune result, STRICT Implementatation, Mixed workload, Inserts use RDTSCP,
// 2 threads, 1m elements
constexpr int DATA_SIZE_T = 16;
constexpr int IDX_SIZE_T = 16;

/// Typedef the skipvector type, to make things easier in the rest of the code
typedef skipvector_pq_teams<ENTRY_T, IDX_SIZE_T, DATA_SIZE_T, MAX_TEAMS_T,
                            hp_manager<MAX_THREADS>>
    sv_teams_t;
