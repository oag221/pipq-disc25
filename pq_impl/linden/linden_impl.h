/*************************************************************************
 * prioq.c
 *
 * Lock-free concurrent priority queue.
 *
 * Copyright (c) 2012-2013, Jonatan Linden
 *
 * Adapted from Keir Fraser's skiplist,
 * Copyright (c) 2001-2003, Keir Fraser
 *
 * Keir Fraser's skiplist is available at
 * http://www.cl.cam.ac.uk/research/srg/netos/lock-free/.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 *  * The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>

/* interface, constant defines, and typedefs */
/* (includes some utilities (e.g. memory barriers)) */
#include "linden.h"

// begin linden_common.cc
pid_t gettid(void) { return (pid_t)syscall(SYS_gettid); }

#ifdef LINDEN_PIN // defined in test.c
void pin(pid_t t, int cpu) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  E_en(sched_setaffinity(t, sizeof(cpu_set_t), &cpuset));
}
#endif // PIN

void gettime(struct timespec *ts) { E(clock_gettime(CLOCK_MONOTONIC, ts)); }

struct timespec timediff(struct timespec begin, struct timespec end) {
  struct timespec tmp;
  if ((end.tv_nsec - begin.tv_nsec) < 0) {
    tmp.tv_sec = end.tv_sec - begin.tv_sec - 1;
    tmp.tv_nsec = 1000000000 + end.tv_nsec - begin.tv_nsec;
  } else {
    tmp.tv_sec = end.tv_sec - begin.tv_sec;
    tmp.tv_nsec = end.tv_nsec - begin.tv_nsec;
  }
  return tmp;
}

// end linden_common.cc

int gc_id[NUM_LEVELS];
__thread ptst_t *ptst;
__thread ptst_simple_t *ptst_simple;

/* initialize new node */
// node_t* linden_ns::linden::alloc_node(pq_t *) { // WITH GC
//   node_t *n;
//   int level = 1;
//   /* crappy rng */
//   unsigned int r = ptst->rand;
//   ptst->rand = r * 1103515245 + 12345;
//   r &= (1u << (NUM_LEVELS - 1)) - 1;
//   /* uniformly distributed bits => geom. dist. level, p = 0.5 */
//   while ((r >>= 1) & 1)
//     ++level;
//   assert(1 <= level && level <= 32);

//   n = (node_t *)gc_alloc(ptst, gc_id[level - 1]);
//   n->level = level;
//   n->inserting = 1;
//   /* necessary to make one of the unit tests to work properly */
//   memset(n->next, 0, level * sizeof(node_t *));
//   return n;
// }

node_t* linden_ns::linden::alloc_node(pq_t *) { // WITHOUT GC
  node_t *n;
  int level = 1;
  /* crappy rng */
  unsigned int r = ptst_simple->rand;
  ptst_simple->rand = r * 1103515245 + 12345;
  r &= (1u << (NUM_LEVELS - 1)) - 1;
  /* uniformly distributed bits => geom. dist. level, p = 0.5 */
  while ((r >>= 1) & 1)
    ++level;
  assert(1 <= level && level <= 32);

  //n = (node_t *)gc_alloc(ptst, gc_id[level - 1]);
  n = (node_t *)malloc((1 + level) * sizeof(node_t));
  n->level = level;
  n->inserting = 1;
  /* necessary to make one of the unit tests to work properly */
  memset(n->next, 0, level * sizeof(node_t *));
  return n;
}

/* Mark node as ready for reclamation to the garbage collector. */
// void linden_ns::linden::free_node(node_t *n) {
//   gc_free(ptst, (void *)n, gc_id[(n->level) - 1]);
// }

void linden_ns::linden::free_node(node_t *n) {
  free(n);
}

/***** locate_preds *****
 * Record predecessors and non-deleted successors of key k.  If k is
 * encountered during traversal of list, the node will be in succs[0].
 *
 * To detect skew in insert operation, return a pointer to the only
 * deleted node not having it's delete flag set.
 *
 * Skew example illustration, when locating 3. Level 1 is shifted in
 * relation to level 0, due to not noticing that s[1] is deleted until
 * level 0 is reached. (pointers in illustration are implicit, e.g.,
 * 0 --> 7 at level 2.)
 *
 *                   del
 *                   p[0]
 * p[2]  p[1]        s[1]  s[0]  s[2]
 *  |     |           |     |     |
 *  v     |           |     |     v
 *  _     v           v     |     _
 * | |    _           _	    v    | |
 * | |   | |    _    | |    _    | |
 * | |   | |   | |   | |   | |   | |
 *  0     1     2     4     6     7
 *  d     d     d
 *
 */

node_t* linden_ns::linden::locate_preds(pkey_t k, node_t **preds, node_t **succs) {
  node_t *x, *x_next, *del = NULL;
  int d = 0, i;

  x = pq->head;
  i = NUM_LEVELS - 1;
  while (i >= 0) {
    x_next = x->next[i];
    d = is_marked_ref(x_next);
    x_next = (node_t *)get_unmarked_ref(x_next);
    assert(x_next != NULL);

    while (x_next->k < k || is_marked_ref(x_next->next[0]) || ((i == 0) && d)) {
      /* Record bottom level deleted node not having delete flag
       * set, if traversed. */
      if (i == 0 && d)
        del = x_next;
      x = x_next;
      x_next = x->next[i];
      d = is_marked_ref(x_next);
      x_next = (node_t *)get_unmarked_ref(x_next);
      assert(x_next != NULL);
    }
    preds[i] = x;
    succs[i] = x_next;
    i--;
  }
  return del;
}

/***** insert *****
 * Insert a new node n with key k and value v.
 * The node will not be inserted if another node with key k is already
 * present in the list.
 *
 * The predecessors, preds, and successors, succs, at all levels are
 * recorded, after which the node n is inserted from bottom to
 * top. Conditioned on that succs[i] is still the successor of
 * preds[i], n will be spliced in on level i.
 */
int linden_ns::linden::insert_linden(pkey_t k, pval_t v) {
  node_t *preds[NUM_LEVELS], *succs[NUM_LEVELS];
  node_t *nnew = NULL, *del = NULL;
  int retVal = 1;

  assert(SENTINEL_KEYMIN < k && k < SENTINEL_KEYMAX);
  //critical_enter();
  if (ptst_simple == NULL) {
    critical_enter_simple();
  }

  /* Initialise a new node for insertion. */
  nnew = alloc_node(pq);
  nnew->k = k;
  nnew->v = v;

  /* lowest level insertion retry loop */
retry:
  del = locate_preds(k, preds, succs);
  int i = 1;
  /* return if key already exists, i.e., is present in a non-deleted
   * node */
  if (succs[0]->k == k && succs[0]->v == v && //! set if you remove the value field
      !is_marked_ref(preds[0]->next[0]) && preds[0]->next[0] == succs[0]) {
    nnew->inserting = 0;
    free_node(nnew);
    retVal = 0;
    goto out;
    //return 0;
  }
  nnew->next[0] = succs[0];

  /* The node is logically inserted once it is present at the bottom
   * level. */
  if (!__sync_bool_compare_and_swap(&preds[0]->next[0], succs[0], nnew)) {
    /* either succ has been deleted (modifying preds[0]),
     * or another insert has succeeded or preds[0] is head,
     * and a restructure operation has updated it */
    goto retry;
  }

  /* Insert at each of the other levels in turn. */
  while (i < nnew->level) {
    /* If successor of new is deleted, we're done. (We're done if
     * only new is deleted as well, but this we can't tell) If a
     * candidate successor at any level is deleted, we consider
     * the operation completed. */
    if (is_marked_ref(nnew->next[0]) || is_marked_ref(succs[i]->next[0]) ||
        del == succs[i])
      goto success;

    /* prepare next pointer of new node */
    nnew->next[i] = succs[i];
    if (!__sync_bool_compare_and_swap(&preds[i]->next[i], succs[i], nnew)) {
      /* failed due to competing insert or restruct */
      del = locate_preds(k, preds, succs);

      /* if new has been deleted, we're done */
      if (succs[0] != nnew)
        goto success;

    } else {
      /* Succeeded at this level. */
      i++;
    }
  }
success:
  if (nnew) {
    IWMB(); /* this flag must be reset after all CAS have completed */
    nnew->inserting = 0;
  }

out:
  //critical_exit();
  return retVal;
}

/***** restructure *****
 *
 * Update the head node's pointers from level 1 and up. Will locate
 * the last node at each level that has the delete flag set, and set
 * the head to point to the successor of that node. After completion,
 * if operating in isolation, for each level i, it holds that
 * head->next[i-1] is before or equal to head->next[i].
 *
 * Illustration valid state after completion:
 *
 *             h[0]  h[1]  h[2]
 *              |     |     |
 *              |     |     v
 *  _           |     v     _
 * | |    _     v     _	   | |
 * | |   | |    _    | |   | |
 * | |   | |   | |   | |   | |
 *  d     d
 *
 */
void linden_ns::linden::restructure() {
  node_t *pred, *cur, *h;
  int i = NUM_LEVELS - 1;

  pred = pq->head;
  while (i > 0) {
    h = pq->head->next[i]; /* record observed head */
    IRMB();                /* the order of these reads must be maintained */
    cur = pred->next[i];   /* take one step forward from pred */
    if (!is_marked_ref(h->next[0])) {
      i--;
      continue;
    }
    /* traverse level until non-marked node is found
     * pred will always have its delete flag set
     */
    while (is_marked_ref(cur->next[0])) {
      pred = cur;
      cur = pred->next[i];
    }
    assert(is_marked_ref(pred->next[0]));

    /* swing head pointer */
    if (__sync_bool_compare_and_swap(&pq->head->next[i], h, cur))
      i--;
  }
}

/* deletemin
 *
 * Delete element with smallest key in queue.
 * Try to update the head node's pointers, if offset > max_offset.
 *
 * Traverse level 0 next pointers until one is found that does
 * not have the delete bit set.
 */
// pval_t deletemin(thread_data_t *d) {
//   pkey_t key;
//   return deletemin_key(pq, &key, d);
// }

// pval_t deletemin_key(pkey_t *key, thread_data_t *d) {
//   pval_t v = 0;
//   node_t *x, *nxt, *obs_head = NULL, *newhead, *cur;
//   int offset, lvl;

//   newhead = NULL;
//   offset = lvl = 0;

//   critical_enter();

//   x = pq->head;
//   obs_head = x->next[0];

//   do {
//     offset++;

//     /* expensive, high probability that this cache line has
//      * been modified */
//     nxt = x->next[0];

//     // tail cannot be deleted
//     if (get_unmarked_ref(nxt) == pq->tail) {
//       *key = -1; // empty flag
//       goto out;
//     }

//     /* Do not allow head to point past a node currently being
//      * inserted. This makes the lock-freedom quite a theoretic
//      * matter. */
//     if (newhead == NULL && x->inserting)
//       newhead = x;

//     /* optimization */
//     if (is_marked_ref(nxt))
//       continue;
//     /* the marker is on the preceding pointer */
//     /* linearisation point deletemin */
//     nxt = __sync_fetch_and_or(&x->next[0], 1);
//     if (is_marked_ref(nxt)) {
//       d->nb_collisions++;
//     }
//   } while ((x = (node_t *)get_unmarked_ref(nxt)) && is_marked_ref(nxt));

//   assert(!is_marked_ref(x));

//   *key = x->k;
//   v = x->v;

//   /* If no inserting node was traversed, then use the latest
//    * deleted node as the new lowest-level head pointed node
//    * candidate. */
//   if (newhead == NULL)
//     newhead = x;

//   /* if the offset is big enough, try to update the head node and
//    * perform memory reclamation */
//   if (offset <= MAX_OFFSET)
//     goto out;

//   /* Optimization. Marginally faster */
//   if (pq->head->next[0] != obs_head)
//     goto out;

//   /* try to swing the lowest level head pointer to point to newhead,
//    * which is deleted */
//   if (__sync_bool_compare_and_swap(&pq->head->next[0], obs_head,
//                                    get_marked_ref(newhead))) {
//     /* Update higher level pointers. */
//     restructure(pq);

//     /* We successfully swung the upper head pointer. The nodes
//      * between the observed head (obs_head) and the new bottom
//      * level head pointed node (newhead) are guaranteed to be
//      * non-live. Mark them for recycling. */

//     cur = (node_t *)get_unmarked_ref(obs_head);
//     while (cur != (node_t *)get_unmarked_ref(newhead)) {
//       nxt = (node_t *)get_unmarked_ref(cur->next[0]);
//       assert(is_marked_ref(cur->next[0]));
//       free_node(cur);
//       cur = nxt;
//     }
//   }
// out:
//   critical_exit();
//   return v;
// }

/*
  copy without thread_data object
*/
pval_t linden_ns::linden::deletemin_key(pkey_t *key) {
  pval_t v = 0;
  node_t *x, *nxt, *obs_head = NULL, *newhead, *cur;
  int offset, lvl;

  newhead = NULL;
  offset = lvl = 0;

  //critical_enter();
  if (ptst_simple == NULL) {
    critical_enter_simple();
  }

  x = pq->head;
  obs_head = x->next[0];

  do {
    offset++;

    /* expensive, high probability that this cache line has
     * been modified */
    nxt = x->next[0];

    // tail cannot be deleted
    if (get_unmarked_ref(nxt) == pq->tail) {
      *key = -1; // empty flag
      goto out;
    }

    /* Do not allow head to point past a node currently being
     * inserted. This makes the lock-freedom quite a theoretic
     * matter. */
    if (newhead == NULL && x->inserting)
      newhead = x;

    /* optimization */
    if (is_marked_ref(nxt))
      continue;
    /* the marker is on the preceding pointer */
    /* linearisation point deletemin */
    nxt = __sync_fetch_and_or(&x->next[0], 1);
    // if (is_marked_ref(nxt)) {
    //   d->nb_collisions++;
    // }
  } while ((x = (node_t *)get_unmarked_ref(nxt)) && is_marked_ref(nxt));

  assert(!is_marked_ref(x));

  *key = x->k;
  v = x->v;

  /* If no inserting node was traversed, then use the latest
   * deleted node as the new lowest-level head pointed node
   * candidate. */
  if (newhead == NULL)
    newhead = x;

  /* if the offset is big enough, try to update the head node and
   * perform memory reclamation */
  if (offset <= MAX_OFFSET)
    goto out;

  /* Optimization. Marginally faster */
  if (pq->head->next[0] != obs_head)
    goto out;

  /* try to swing the lowest level head pointer to point to newhead,
   * which is deleted */
  if (__sync_bool_compare_and_swap(&pq->head->next[0], obs_head,
                                   get_marked_ref(newhead))) {
    /* Update higher level pointers. */
    restructure();

    /* We successfully swung the upper head pointer. The nodes
     * between the observed head (obs_head) and the new bottom
     * level head pointed node (newhead) are guaranteed to be
     * non-live. Mark them for recycling. */

    // cur = (node_t *)get_unmarked_ref(obs_head);
    // while (cur != (node_t *)get_unmarked_ref(newhead)) {
    //   nxt = (node_t *)get_unmarked_ref(cur->next[0]);
    //   assert(is_marked_ref(cur->next[0]));
    //   free_node(cur);
    //   cur = nxt;
    // }
  }
out:
  //critical_exit();
  return v;
}

/*
 * Init structure, setup sentinel head and tail nodes.
 */
pq_t* linden_ns::linden::pq_init() {
  node_t *t, *h;
  int i;

  //_init_gc_subsystem();

  /* head and tail nodes */
  t = (node_t *)malloc(sizeof *t + (NUM_LEVELS - 1) * sizeof(node_t *));
  h = (node_t *)malloc(sizeof *h + (NUM_LEVELS - 1) * sizeof(node_t *));

  t->inserting = 0;
  h->inserting = 0;

  t->k = SENTINEL_KEYMAX;
  h->k = SENTINEL_KEYMIN;
  h->level = NUM_LEVELS;
  t->level = NUM_LEVELS;

  for (i = 0; i < NUM_LEVELS; i++)
    h->next[i] = t;

  pq = (pq_t *)malloc(sizeof *pq);
  pq->head = h;
  pq->tail = t;
  //MAX_OFFSET = max_offset;

  // for (i = 0; i < NUM_LEVELS; i++)
  //   gc_id[i] = gc_add_allocator(sizeof(node_t) + i * sizeof(node_t *));

  return pq;
}

/* Cleanup, mark all the nodes for recycling. */
void linden_ns::linden::pq_destroy() {
  node_t *cur, *pred;
  cur = pq->head;
  while (cur != pq->tail) {
    pred = cur;
    cur = (node_t *)get_unmarked_ref(pred->next[0]);
    free_node(pred);
  }
  free(pq->tail);
  free(pq->head);
  free(pq);
}

int linden_ns::linden::empty() { return (pq->head->next[0] == pq->tail); }

long long linden_ns::linden::getSize() {
  node_t *cur, *nxt;
  cur = pq->head;
  do {
    /* expensive, high probability that this cache line has
     * been modified */
    nxt = cur->next[0];

    // tail cannot be deleted
    if (get_unmarked_ref(nxt) == pq->tail) {
      break;
    }

  } while ((cur = (node_t *)get_unmarked_ref(nxt)) && is_marked_ref(nxt));


  long long size = 0; // bc don't want to include the head
  while (1) {
    size += 1;
    cur = (node_t *)get_unmarked_ref(cur->next[0]);
    if (cur == pq->tail) {
      break;
    }
  }
  return size;
}


std::string linden_ns::linden::getSizeString() {
  return std::to_string(getSize());
}

long long linden_ns::linden::getKeySum() {
  node_t *cur, *nxt;
  cur = pq->head;
  do {
    /* expensive, high probability that this cache line has
     * been modified */
    nxt = cur->next[0];

    // tail cannot be deleted
    if (get_unmarked_ref(nxt) == pq->tail) {
      break;
    }

  } while ((cur = (node_t *)get_unmarked_ref(nxt)) && is_marked_ref(nxt));

  long long sum = 0;
  while (1) {
    sum += cur->k;
    //std::cout << "cur->k = " << cur->k << "\n";
    cur = (node_t *)get_unmarked_ref(cur->next[0]);
    if (cur == pq->tail) {
      break;
    }
  }
  return sum;
}

long long linden_ns::linden::debugKeySum() {
  node_t *cur, *nxt;
  cur = pq->head;
  do {
    /* expensive, high probability that this cache line has
     * been modified */
    nxt = cur->next[0];

    // tail cannot be deleted
    if (get_unmarked_ref(nxt) == pq->tail) {
      break;
    }

  } while ((cur = (node_t *)get_unmarked_ref(nxt)) && is_marked_ref(nxt));

  long long sum = 0;
  while (1) {
    sum += cur->k;
    //std::cout << "cur->k = " << cur->k << "\n";
    cur = (node_t *)get_unmarked_ref(cur->next[0]);
    if (cur == pq->tail) {
      break;
    }
  }
  return sum;
}

bool linden_ns::linden::getValidated() {
  return true;
}