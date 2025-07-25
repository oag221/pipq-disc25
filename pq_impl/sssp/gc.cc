/******************************************************************************
 * gc.c
 *
 * A fully recycling epoch-based garbage collector. Works by counting threads in
 * and out of critical regions, to work out when garbage queues can be fully
 * deleted.
 *
 * Copyright (c) 2001-2003, K A Fraser
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "include/gc.h"
#include "include/portable_defns.h"

/* Recycled nodes are filled with this value if WEAK_MEM_ORDER. */
#define INVALID_BYTE 0
#define INITIALISE_NODES(_p, _c) memset((_p), INVALID_BYTE, (_c));

/* Number of unique block sizes we can deal with. */
#define MAX_SIZES 32

#define MAX_HOOKS 4

/*
 * The initial number of allocation chunks for each per-blocksize list.
 * Popular allocation lists will steadily increase the allocation unit
 * in line with demand.
 */
#define ALLOC_CHUNKS_PER_LIST 10

/*
 * How many times should a thread call gc_enter(), seeing the same epoch
 * each time, before it makes a reclaim attempt?
 */
#define ENTRIES_PER_RECLAIM_ATTEMPT 100

/*
 *  0: current epoch -- threads are moving to this;
 * -1: some threads may still throw garbage into this epoch;
 * -2: no threads can see this epoch => we can zero garbage lists;
 * -3: all threads see zeros in these garbage lists => move to alloc lists.
 */
#define NR_EPOCHS 3

/*
 * A chunk amortises the cost of allocation from shared lists. It also
 * helps when zeroing nodes, as it increases per-cacheline pointer density
 * and means that node locations don't need to be brought into the cache
 * (most architectures have a non-temporal store instruction).
 */
#define BLKS_PER_CHUNK 100
struct chunk_t {
  chunk_t *next;  /* chunk chaining                 */
  unsigned int i; /* the next entry in blk[] to use */
  void *blk[BLKS_PER_CHUNK];
};

static struct gc_global_st {
  CACHE_PAD(0);

  /* The current epoch. */
  VOLATILE unsigned int current;
  CACHE_PAD(1);

  /* Exclusive access to gc_reclaim(). */
  VOLATILE unsigned int inreclaim;
  CACHE_PAD(2);

  /*
   * RUN-TIME CONSTANTS (to first approximation)
   */

  /* Memory page size, in bytes. */
  unsigned int page_size;

  /* Node sizes (run-time constants). */
  int nr_sizes;
  int blk_sizes[MAX_SIZES];

  /* Registered epoch hooks. */
  int nr_hooks;
  hook_fn_t hook_fns[MAX_HOOKS];
  CACHE_PAD(3);

  /*
   * DATA WE MAY HIT HARD
   */

  /* Chain of free, empty chunks. */
  chunk_t *VOLATILE free_chunks;

  /* Main allocation lists. */
  chunk_t *VOLATILE alloc[MAX_SIZES];
  VOLATILE unsigned int alloc_size[MAX_SIZES];
} gc_global;

/* Per-thread state. */
struct gc_t {
  /* Epoch that this thread sees. */
  unsigned int epoch;

  /* Number of calls to gc_entry() since last gc_reclaim() attempt. */
  unsigned int entries_since_reclaim;

  /* Used by gc_async_barrier(). */
  void *async_page;
  int async_page_state;

  /* Garbage lists. */
  chunk_t *garbage[NR_EPOCHS][MAX_SIZES];
  chunk_t *garbage_tail[NR_EPOCHS][MAX_SIZES];
  chunk_t *chunk_cache;

  /* Local allocation lists. */
  chunk_t *alloc[MAX_SIZES];
  unsigned int alloc_chunks[MAX_SIZES];

  /* Hook pointer lists. */
  chunk_t *hook[NR_EPOCHS][MAX_HOOKS];
};

#define MEM_FAIL(_s)                                                           \
  do {                                                                         \
    std::cerr << "OUT OF MEMORY: " << (_s) << " bytes at line " << __LINE__    \
              << "\n";                                                         \
    exit(1);                                                                   \
  } while (0)

/* Allocate more empty chunks from the heap. */
#define CHUNKS_PER_ALLOC 1000
static chunk_t *alloc_more_chunks(void) {
  int i;
  chunk_t *h, *p;

  h = p = (chunk_t *)ALIGNED_ALLOC(CHUNKS_PER_ALLOC * sizeof(*h));
  if (h == NULL)
    MEM_FAIL(CHUNKS_PER_ALLOC * sizeof(*h));

  for (i = 1; i < CHUNKS_PER_ALLOC; i++) {
    p->next = p + 1;
    p++;
  }

  p->next = h;

  return (h);
}

/* Put a chain of chunks onto a list. */
static void add_chunks_to_list(chunk_t *ch, chunk_t *head) {
  chunk_t *h_next, *new_h_next, *ch_next;
  ch_next = ch->next;
  new_h_next = head->next;
  do {
    ch->next = h_next = new_h_next;
    WMB_NEAR_CAS();
  } while ((new_h_next = CASPO(&head->next, h_next, ch_next)) != h_next);
}

/* Allocate a chain of @n empty chunks. Pointers may be garbage. */
static chunk_t *get_empty_chunks(int n) {
  int i;
  chunk_t *new_rh, *rh, *rt, *head;

retry:
  head = gc_global.free_chunks;
  new_rh = head->next;
  do {
    rh = new_rh;
    rt = head;
    WEAK_DEP_ORDER_RMB();
    for (i = 0; i < n; i++) {
      if ((rt = rt->next) == head) {
        /* Allocate some more chunks. */
        add_chunks_to_list(alloc_more_chunks(), head);
        goto retry;
      }
    }
  } while ((new_rh = CASPO(&head->next, rh, rt->next)) != rh);

  rt->next = rh;
  return (rh);
}

/* Get @n filled chunks, pointing at blocks of @sz bytes each. */
static chunk_t *get_filled_chunks(int n, int sz) {
  chunk_t *h, *p;
  char *node;
  int i;

  node = (char *)ALIGNED_ALLOC(n * BLKS_PER_CHUNK * sz);
  if (node == NULL)
    MEM_FAIL(n * BLKS_PER_CHUNK * sz);

  h = p = get_empty_chunks(n);
  do {
    p->i = BLKS_PER_CHUNK;
    for (i = 0; i < BLKS_PER_CHUNK; i++) {
      p->blk[i] = node;
      node += sz;
    }
  } while ((p = p->next) != h);

  return (h);
}

/*
 * gc_async_barrier: Cause an asynchronous barrier in all other threads. We do
 * this by causing a TLB shootdown to be propagated to all other processors.
 * Each time such an action is required, this function calls:
 *   mprotect(async_page, <page size>, <new flags>)
 * Each thread's state contains a memory page dedicated for this purpose.
 */
#ifdef WEAK_MEM_ORDER
static void gc_async_barrier(gc_t *gc) {
  mprotect(gc->async_page, gc_global.page_size,
           gc->async_page_state ? PROT_READ : PROT_NONE);
  gc->async_page_state = !gc->async_page_state;
}
#else
#define gc_async_barrier(_g) ((void)0)
#endif

/* Grab a level @i allocation chunk from main chain. */
static chunk_t *get_alloc_chunk(gc_t *, int i) {
  chunk_t *alloc, *p, *new_p, *nh;
  unsigned int sz;

  alloc = gc_global.alloc[i];
  new_p = alloc->next;

  do {
    p = new_p;
    while (p == alloc) {
      sz = gc_global.alloc_size[i];
      nh = get_filled_chunks(sz, gc_global.blk_sizes[i]);
      ADD_TO(gc_global.alloc_size[i], sz >> 3);
      gc_async_barrier(gc);
      add_chunks_to_list(nh, alloc);
      p = alloc->next;
    }
    WEAK_DEP_ORDER_RMB();
  } while ((new_p = CASPO(&alloc->next, p, p->next)) != p);

  p->next = p;
  assert(p->i == BLKS_PER_CHUNK);
  return (p);
}

/*
 * gc_reclaim: Scans the list of struct gc_perthread looking for the lowest
 * maximum epoch number seen by a thread that's in the list code. If it's the
 * current epoch, the "nearly-free" lists from the previous epoch are
 * reclaimed, and the epoch is incremented.
 */
static void gc_reclaim(ptst_t *our_ptst) {
  ptst_t *ptst, *first_ptst; //, *our_ptst = NULL;
  gc_t *gc = NULL;
  unsigned long curr_epoch;
  chunk_t *ch, *t;
  int three_ago;

  /* Barrier to entering the reclaim critical section. */
  if (gc_global.inreclaim || CASIO(&gc_global.inreclaim, 0, 1))
    return;

  /*
   * Grab first ptst structure *before* barrier -- prevent bugs
   * on weak-ordered architectures.
   */
  first_ptst = ptst_first();
  MB();
  curr_epoch = gc_global.current;

  /* Have all threads seen the current epoch, or not in mutator code? */
  for (ptst = first_ptst; ptst != NULL; ptst = ptst_next(ptst)) {
    if ((ptst->count > 1) && (ptst->gc->epoch != curr_epoch))
      goto out;
  }

  /*
   * Three-epoch-old garbage lists move to allocation lists.
   * Two-epoch-old garbage lists are cleaned out.
   *
   * [mfs] I don't see where two-epoch-old garbage is handled
   */
  three_ago = (curr_epoch + 1) % NR_EPOCHS;
  // if ( gc_global.nr_hooks != 0 )
  // our_ptst = (ptst_t *)pthread_getspecific(ptst_key);
  for (ptst = first_ptst; ptst != NULL; ptst = ptst_next(ptst)) {
    gc = ptst->gc;

    for (auto i = 0; i < gc_global.nr_sizes; i++) {
      /* NB. Leave one chunk behind, as it is probably not yet full. */
      t = gc->garbage[three_ago][i];
      if ((t == NULL) || ((ch = t->next) == t))
        continue;
      gc->garbage_tail[three_ago][i]->next = ch;
      gc->garbage_tail[three_ago][i] = t;
      t->next = t;
      add_chunks_to_list(ch, gc_global.alloc[i]);
    }

    for (auto i = 0; i < gc_global.nr_hooks; i++) {
      hook_fn_t fn = gc_global.hook_fns[i];
      ch = gc->hook[three_ago][i];
      if (ch == NULL)
        continue;
      gc->hook[three_ago][i] = NULL;

      t = ch;
      do {
        for (unsigned int j = 0; j < t->i; j++)
          fn(our_ptst, t->blk[j]);
      } while ((t = t->next) != ch);

      add_chunks_to_list(ch, gc_global.free_chunks);
    }
  }

  /* Update current epoch. */
  WMB();
  gc_global.current = (curr_epoch + 1) % NR_EPOCHS;

out:
  gc_global.inreclaim = 0;
}

void *gc_alloc(ptst_t *ptst, int alloc_id) {
  gc_t *gc = ptst->gc;
  chunk_t *ch;

  ch = gc->alloc[alloc_id];
  if (ch->i == 0) {
    if (gc->alloc_chunks[alloc_id]++ == 100) {
      gc->alloc_chunks[alloc_id] = 0;
      add_chunks_to_list(ch, gc_global.free_chunks);
      gc->alloc[alloc_id] = ch = get_alloc_chunk(gc, alloc_id);
    } else {
      chunk_t *och = ch;
      ch = get_alloc_chunk(gc, alloc_id);
      ch->next = och->next;
      och->next = ch;
      gc->alloc[alloc_id] = ch;
    }
  }

  return ch->blk[--ch->i];
}

static chunk_t *chunk_from_cache(gc_t *gc) {
  chunk_t *ch = gc->chunk_cache, *p = ch->next;

  if (ch == p) {
    gc->chunk_cache = get_empty_chunks(100);
  } else {
    ch->next = p->next;
    p->next = p;
  }

  p->i = 0;
  return (p);
}

void gc_free(ptst_t *ptst, void *p, int alloc_id) {
  gc_t *gc = ptst->gc;
  chunk_t *prev, *nnew, *ch = gc->garbage[gc->epoch][alloc_id];

  if (ch == NULL) {
    gc->garbage[gc->epoch][alloc_id] = ch = chunk_from_cache(gc);
    gc->garbage_tail[gc->epoch][alloc_id] = ch;
  } else if (ch->i == BLKS_PER_CHUNK) {
    prev = gc->garbage_tail[gc->epoch][alloc_id];
    nnew = chunk_from_cache(gc);
    gc->garbage[gc->epoch][alloc_id] = nnew;
    nnew->next = ch;
    prev->next = nnew;
    ch = nnew;
  }

  ch->blk[ch->i++] = p;
}

void gc_add_ptr_to_hook_list(ptst_t *ptst, void *ptr, int hook_id) {
  gc_t *gc = ptst->gc;
  chunk_t *och, *ch = gc->hook[gc->epoch][hook_id];

  if (ch == NULL) {
    gc->hook[gc->epoch][hook_id] = ch = chunk_from_cache(gc);
  } else {
    ch = ch->next;
    if (ch->i == BLKS_PER_CHUNK) {
      och = gc->hook[gc->epoch][hook_id];
      ch = chunk_from_cache(gc);
      ch->next = och->next;
      och->next = ch;
    }
  }

  ch->blk[ch->i++] = ptr;
}

void gc_unsafe_free(ptst_t *ptst, void *p, int alloc_id) {
  gc_t *gc = ptst->gc;
  chunk_t *ch;

  ch = gc->alloc[alloc_id];
  if (ch->i < BLKS_PER_CHUNK) {
    ch->blk[ch->i++] = p;
  } else {
    gc_free(ptst, p, alloc_id);
  }
}

void gc_enter(ptst_t *ptst) {
  gc_t *gc = ptst->gc;
  int new_epoch, cnt;

retry:
  cnt = ptst->count++;
  MB();
  if (cnt == 1) {
    new_epoch = gc_global.current;
    if (gc->epoch != (unsigned int)new_epoch) {
      gc->epoch = new_epoch;
      gc->entries_since_reclaim = 0;
    } else if (gc->entries_since_reclaim++ == 100) {
      ptst->count--;
      gc->entries_since_reclaim = 0;
      gc_reclaim(ptst);
      goto retry;
    }
  }
}

void gc_exit(ptst_t *ptst) {
  MB();
  ptst->count--;
}

gc_t *gc_init(void) {
  gc_t *gc;
  int i;

  gc = (gc_t *)ALIGNED_ALLOC(sizeof(*gc));
  if (gc == NULL)
    MEM_FAIL(sizeof(*gc));
  memset(gc, 0, sizeof(*gc));

  gc->chunk_cache = get_empty_chunks(100);

  /* Get ourselves a set of allocation chunks. */
  for (i = 0; i < gc_global.nr_sizes; i++) {
    gc->alloc[i] = get_alloc_chunk(gc, i);
  }
  for (; i < MAX_SIZES; i++) {
    gc->alloc[i] = chunk_from_cache(gc);
  }

  return (gc);
}

int gc_add_allocator(int alloc_size) {
  int ni, i = gc_global.nr_sizes;
  while ((ni = CASIO(&gc_global.nr_sizes, i, i + 1)) != i)
    i = ni;
  gc_global.blk_sizes[i] = alloc_size;
  gc_global.alloc_size[i] = ALLOC_CHUNKS_PER_LIST;
  gc_global.alloc[i] = get_filled_chunks(ALLOC_CHUNKS_PER_LIST, alloc_size);
  return i;
}

void gc_remove_allocator(int) { /* This is a no-op for now. */
}

int gc_add_hook(hook_fn_t fn) {
  int ni, i = gc_global.nr_hooks;
  while ((ni = CASIO(&gc_global.nr_hooks, i, i + 1)) != i)
    i = ni;
  gc_global.hook_fns[i] = fn;
  return i;
}

void gc_remove_hook(int) { /* This is a no-op for now. */
}

void _destroy_gc_subsystem(void) {}

void _init_gc_subsystem(void) {
  memset(&gc_global, 0, sizeof(gc_global));

  gc_global.page_size = (unsigned int)sysconf(_SC_PAGESIZE);
  gc_global.free_chunks = alloc_more_chunks();

  gc_global.nr_hooks = 0;
  gc_global.nr_sizes = 0;
}
