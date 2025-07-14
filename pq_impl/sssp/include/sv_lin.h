#pragma once

/// Instantiating the SVPQ is revealing some flaws in the design.  The most
/// significant is that std::atomic<> of a non-simple type is going to be very
/// slow, which will harm insertions.  Is "unique key" just not reasonable in
/// SkipVector?
///
/// Another issue is that -Werror is causing compilation issues in the SVPQ
/// code.

#include "../../common/entry.h"
#include "../../common/machine_defines.h"
#include "../../hp/hp_manager.h"
#include "../../skipvector-pq/skipvector_pq.h"

constexpr slkey_t EMPTY_S = static_cast<slkey_t>(-1);
using ENTRY_S = entry<slkey_t, val_t, EMPTY_S>;

// Autotune result, Strict Implementatation, Mixed workload, Inserts use RDTSCP,
// 2 threads, 1m elements
constexpr int DATA_SIZE_S = 16;
constexpr int IDX_SIZE_S = 16;

/// Typedef the skipvector type, to make things easier in the rest of the code
typedef skipvector_pq<ENTRY_S, IDX_SIZE_S, DATA_SIZE_S, hp_manager<MAX_THREADS>>
    sv_lin_t;
