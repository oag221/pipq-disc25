#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

const test_type NO_VALUE = -1;
const test_type KEY_MIN = numeric_limits<test_type>::min()+1;
const test_type KEY_MAX = numeric_limits<test_type>::max()-1; // must be less than max(), because the snap collector needs a reserved key larger than this!

#define VALUE key 
#define KEY key
#define VALUE_TYPE test_type

#if defined(NUMA_PQ)
    #include "numa_pq_impl.h"
    #include "record_manager.h"
    using namespace pq_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC insert_wrapper
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC hier_delete
    #endif

    #define DS_DECLARATION pq<test_type, test_type, MEMMGMT_T>
    #define MEMMGMT_T record_manager<RECLAIM, ALLOC, POOL, pq_ns::PQ_Node>
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS, KEY_MIN, KEY_MAX, NO_VALUE, glob.rngs)
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS)
    #define DS_CONSTRUCTOR(threads, hls) new DS_DECLARATION(threads, hls)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(tid, key) == true
    #define DELETE_AND_CHECK_SUCCESS ds->REMOVE_MIN_FUNC()
    #define INIT_THREAD(tid) ds->threadInit(tid)
    #define INIT_ALL ds->PQInit();
    #define DEINIT_ALL ds->PQDeinit();
    #define INIT_THREAD_DELETES ds->initThreadLocalMetadata();
    #define GET_TIDX
    #define GET_T_PER_ZONE
    //#define PERFORM_HELP_TO_COMPLETE
    
#elif defined(PIPQ_STRICT)

    #include "pipq_strict_impl.h"
    using namespace pq_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC hier_insert_local
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC hier_delete
    #endif

    #define DS_DECLARATION pq<test_type>
    //#define MEMMGMT_T record_manager<RECLAIM, ALLOC, POOL, pq_ns::PQ_Node>
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS, KEY_MIN, KEY_MAX, NO_VALUE, glob.rngs)
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS)
    #define DS_CONSTRUCTOR(hls, lead_buf_capacity, lead_buf_ideal, tot_threads, counter_threshold, counter_max, max_offset) new DS_DECLARATION(hls, lead_buf_capacity, lead_buf_ideal, tot_threads, counter_threshold, counter_max, max_offset)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key, value) == true
    #define DELETE_AND_CHECK_SUCCESS min_key = ds->REMOVE_MIN_FUNC()
    #define INIT_THREAD(tid) ds->threadInit(tid)
    #define DES_COORD_INIT_THREAD(tid) ds->desCoordThreadInit(tid)
    #define INIT_ALL ds->PQInit();
    #define DEINIT_ALL ds->PQDeinit();
    #define INIT_THREAD_DELETES ds->initThreadLocalMetadata();
    #define GET_TIDX thread_idx = ds->get_tidx();
    #define GET_T_PER_ZONE threads_per_zone = ds->get_local_numa_workers();
    #define GET_AVG_DELMIN_OPS avg_delmin_ops = ds->get_avg_delmin_ops();
    //#define DESG_TD_ROLE ds->desg_td_role();
    //#define PERFORM_HELP_TO_COMPLETE ds->check_help_upsert();

#elif defined(PIPQ_STRICT_ATOMIC)

    #include "pipq_strict_impl.h"
    using namespace pq_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC hier_insert_local
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC hier_delete
    #endif

    #define DS_DECLARATION pq<test_type>
    //#define MEMMGMT_T record_manager<RECLAIM, ALLOC, POOL, pq_ns::PQ_Node>
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS, KEY_MIN, KEY_MAX, NO_VALUE, glob.rngs)
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS)
    #define DS_CONSTRUCTOR(hls, lead_buf_capacity, lead_buf_ideal, tot_threads, counter_threshold, counter_max, max_offset) new DS_DECLARATION(hls, lead_buf_capacity, lead_buf_ideal, tot_threads, counter_threshold, counter_max, max_offset)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key, value) == true
    #define DELETE_AND_CHECK_SUCCESS min_key = ds->REMOVE_MIN_FUNC()
    #define INIT_THREAD(tid) ds->threadInit(tid)
    #define DES_COORD_INIT_THREAD(tid) ds->desCoordThreadInit(tid)
    #define INIT_ALL ds->PQInit();
    #define DEINIT_ALL ds->PQDeinit();
    #define INIT_THREAD_DELETES ds->initThreadLocalMetadata();
    #define GET_TIDX thread_idx = ds->get_tidx();
    #define GET_T_PER_ZONE threads_per_zone = ds->get_local_numa_workers();
    #define GET_AVG_DELMIN_OPS avg_delmin_ops = ds->get_avg_delmin_ops();
    //#define DESG_TD_ROLE ds->desg_td_role();
    //#define PERFORM_HELP_TO_COMPLETE ds->check_help_upsert();

#elif defined(CBPQ)

    #include "ChunkedPriorityQueueImpl.h" // todo should this be here ??
    //using namespace pq_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC insert
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC delmin
    #endif

    #define DS_DECLARATION ChunkedPriorityQueue
    //#define MEMMGMT_T record_manager<RECLAIM, ALLOC, POOL, pq_ns::PQ_Node>
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS, KEY_MIN, KEY_MAX, NO_VALUE, glob.rngs)
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS)
    #define DS_CONSTRUCTOR() new DS_DECLARATION()

    #define INSERT_AND_CHECK_SUCCESS_CBPQ ds->INSERT_FUNC(key, t_inf)
    #define DELETE_AND_CHECK_SUCCESS_CBPQ min_key = ds->REMOVE_MIN_FUNC(t_inf)
    #define INSERT_AND_CHECK_SUCCESS false
    #define DELETE_AND_CHECK_SUCCESS false
    #define INIT_THREAD(tid) 
    #define DES_COORD_INIT_THREAD(tid) ds->desCoordThreadInit(tid)
    #define INIT_ALL
    #define DEINIT_ALL
    #define INIT_THREAD_DELETES
    #define GET_TIDX 
    #define GET_T_PER_ZONE 
    #define GET_AVG_DELMIN_OPS 
    //#define PERFORM_HELP_TO_COMPLETE ds->check_help_upsert();

#elif defined(MOUNDS)
    #include "mounds.h"
    #ifndef INSERT_FUNC
        #define INSERT_FUNC add
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC remove
    #endif

    #define DS_DECLARATION moundpq_t
    #define DS_CONSTRUCTOR() new DS_DECLARATION()

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key)
    #define DELETE_AND_CHECK_SUCCESS min_val = ds->REMOVE_MIN_FUNC()
    #define INIT_THREAD(tid) wbmm_thread_init(tid)
    #define DES_COORD_INIT_THREAD
    #define INIT_ALL wbmm_init(THREADS)
    #define DEINIT_ALL ds->deinit();
    #define INIT_THREAD_DELETES

#elif defined(ZMQ)
    #include "RelaxedConcurrentPriorityQueue.h"
    using namespace folly;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC push
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC pop
    #endif

    #define DS_DECLARATION RelaxedConcurrentPriorityQueue
    #define DS_CONSTRUCTOR() new DS_DECLARATION()

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key)
    #define DELETE_AND_CHECK_SUCCESS ds->REMOVE_MIN_FUNC(min_key)
    #define INIT_THREAD(tid)
    #define DES_COORD_INIT_THREAD
    #define INIT_ALL
    #define DEINIT_ALL
    #define INIT_THREAD_DELETES

#elif defined(LLSL_TEST)

    #include "ll-sl-test_impl.h"
    using namespace llsl_test_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC hier_insert_local
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC hier_delete
    #endif

    #define DS_DECLARATION pq<test_type>
    //#define MEMMGMT_T record_manager<RECLAIM, ALLOC, POOL, pq_ns::PQ_Node>
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS, KEY_MIN, KEY_MAX, NO_VALUE, glob.rngs)
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS)
    #define DS_CONSTRUCTOR(hls, lead_buf_capacity, lead_buf_ideal, tot_threads, counter_threshold, counter_max) new DS_DECLARATION(hls, lead_buf_capacity, lead_buf_ideal, tot_threads, counter_threshold, counter_max)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key, value) == true
    #define DELETE_AND_CHECK_SUCCESS min_key = ds->REMOVE_MIN_FUNC()
    #define INIT_THREAD(tid) ds->threadInit(tid)
    #define DES_COORD_INIT_THREAD(tid) ds->desCoordThreadInit(tid)
    #define INIT_ALL ds->PQInit();
    #define DEINIT_ALL ds->PQDeinit();
    #define INIT_THREAD_DELETES ds->initThreadLocalMetadata();
    #define GET_TIDX thread_idx = ds->get_tidx();
    #define GET_T_PER_ZONE threads_per_zone = ds->get_local_numa_workers();
    #define GET_AVG_DELMIN_OPS avg_delmin_ops = ds->get_avg_delmin_ops();

#elif defined(PIPQ_RELAXED)

    #include "pipq_relaxed_impl.h"
    using namespace pq_multi_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC hier_insert_local
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC hier_delete
    #endif

    #define DS_DECLARATION pq_multi<test_type>
    //#define MEMMGMT_T record_manager<RECLAIM, ALLOC, POOL, pq_ns::PQ_Node>
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS, KEY_MIN, KEY_MAX, NO_VALUE, glob.rngs)
    //#define DS_CONSTRUCTOR new DS_DECLARATION(THREADS)
    #define DS_CONSTRUCTOR(hls, tot_threads, counter_threshold, counter_max) new DS_DECLARATION(hls, tot_threads, counter_threshold, counter_max)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key, value) == true
    #define DELETE_AND_CHECK_SUCCESS min_key = ds->REMOVE_MIN_FUNC()
    #define INIT_THREAD(tid) ds->threadInit(tid)
    #define DES_COORD_INIT_THREAD(tid) ds->desCoordThreadInit(tid)
    #define INIT_ALL ds->PQInit();
    #define DEINIT_ALL ds->PQDeinit();
    #define INIT_THREAD_DELETES ds->initThreadLocalMetadata();
    #define GET_TIDX thread_idx = ds->get_tidx();
    #define GET_T_PER_ZONE threads_per_zone = ds->get_local_numa_workers();
    #define GET_AVG_DELMIN_OPS avg_delmin_ops = ds->get_avg_delmin_ops();

#elif defined(LINDEN)
    #include "linden_impl.h"
    using namespace linden_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC insert_linden
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC deletemin_key
    #endif

    #define DS_DECLARATION linden
    #define DS_CONSTRUCTOR(max_offset, max_level, nthreads) new DS_DECLARATION(max_offset, max_level, nthreads)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key, value) == 1
    #define DELETE_AND_CHECK_SUCCESS ds->REMOVE_MIN_FUNC(&min_key)
    #define INIT_THREAD(tid)
    #define INIT_ALL ds->pq_init();
    #define DEINIT_ALL //ds->pq_destroy(); // TODO: figure out why ptst becomes null...
    #define GET_TIDX
    #define GET_T_PER_ZONE

#elif defined(SPRAY)
    #include "spraylist_impl.h"
    using namespace spray_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC spray_insert
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC spray_delete_min_key
    #endif

    #define DS_DECLARATION spraylist
    #define DS_CONSTRUCTOR(nthreads, max_key) new DS_DECLARATION(nthreads, max_key)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key, value) == 1
    #define DELETE_AND_CHECK_SUCCESS ds->REMOVE_MIN_FUNC(&min_key, &min_val)
    #define INIT_THREAD(tid) ds->thread_init(seed);
    #define INIT_ALL ds->spray_init();
    #define DEINIT_ALL ds->spray_destroy();
    #define GET_TIDX
    #define GET_T_PER_ZONE

#elif defined(SMQ)
    #include "smq_impl.h"
    using namespace smq_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC ins_wrapper
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC del_wrapper
    #endif

    #define DS_DECLARATION StealingMultiQueue<std::pair<long,long>, 8, 8, true>
    #define DS_CONSTRUCTOR(nthreads) new DS_DECLARATION(nthreads)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key, value) == 1
    #define DELETE_AND_CHECK_SUCCESS ds->REMOVE_MIN_FUNC(&min_key)
    #define INIT_THREAD(tid) ds->initThread(tid);
    #define INIT_ALL
    #define DEINIT_ALL
    #define GET_TIDX
    #define GET_T_PER_ZONE

#elif defined(LOTAN)
    #include "lotan_shavit_impl.h"
    using namespace lotan_shavit_ns;

    #ifndef INSERT_FUNC
        #define INSERT_FUNC lotan_fraser_insert
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC lotan_shavit_delete_min_key
    #endif

    #define DS_DECLARATION lotan_shavit
    #define DS_CONSTRUCTOR(num_threads, max_key) new DS_DECLARATION(num_threads, max_key)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(key, value) == true
    #define DELETE_AND_CHECK_SUCCESS ds->REMOVE_MIN_FUNC(&min_key, &min_val)
    #define INIT_THREAD(tid) ds->thread_init();
    #define INIT_ALL ds->lotan_init();
    #define DEINIT_ALL //ds->pq_destroy(); // TODO: figure out why ptst becomes null...
    #define GET_TIDX
    #define GET_T_PER_ZONE

#elif defined(ARRAY_SKIPLIST)
    #include "array_skiplist_pq.h"
    //using namespace spray_ns;

    using descriptor = _ALG<_OREC, clock_policy::_CLOCK>; // _ALG, _OREC, _CLOCK all defined in Makefile

    #ifndef INSERT_FUNC
        #define INSERT_FUNC insert
    #endif

    #ifndef REMOVE_MIN_FUNC
        #define REMOVE_MIN_FUNC extract_min
    #endif

    #define DS_DECLARATION array_skiplist_pq<int, int, _ALG<_OREC, clock_policy::_CLOCK>>
    #define DS_CONSTRUCTOR(optstm) new DS_DECLARATION(optstm)

    #define INSERT_AND_CHECK_SUCCESS ds->INSERT_FUNC(me, key, value)
    #define DELETE_AND_CHECK_SUCCESS min_key = ds->REMOVE_MIN_FUNC(me)
    #define INIT_THREAD(tid) ds->init_thread(tid)
    #define INIT_ALL 
    #define DEINIT_ALL 
    #define GET_TIDX
    #define GET_T_PER_ZONE
    #define DEINIT_THREAD ds->thread_terminate(me);

#else
    #error "Failed to define a data structure"
#endif

#endif /* DATA_STRUCTURE_H */

