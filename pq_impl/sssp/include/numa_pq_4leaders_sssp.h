#pragma once

#include "../../recordmgr/debugprinting.h"
#include "../../harris_ll_4leaders/harris_4leaders.h"
#include "../../pipq-relaxed/pipq_relaxed.h"
#include "../../pipq-relaxed/pipq_relaxed_impl.h"

using namespace pq_multi_ns;

typedef pq_multi<long unsigned> numa_pq_4_t;
// = new pq<long long>(heap_list_size, lead_buf_capacity, lead_buf_ideal, tot_threads);