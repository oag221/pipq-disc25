#pragma once

#include "../../stealing-multi-queue/smq.h"
#include "../../stealing-multi-queue/smq_impl.h"

using namespace smq_ns;

typedef StealingMultiQueue<std::pair<long,long>, 8, 8, true> smq_t;