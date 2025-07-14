#pragma once

#include "../../recordmgr/debugprinting.h"
#include "../../harris_ll/harris.h"
#include "../../pipq-strict/pipq_strict.h"
#include "../../pipq-strict/pipq_strict_impl.h"

using namespace pq_ns;

typedef pq<long unsigned> numa_pq_t;
// = new pq<long long>(heap_list_size, lead_buf_capacity, lead_buf_ideal, tot_threads);