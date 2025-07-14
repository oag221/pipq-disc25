/*
 * File:
 *   fraser.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Lock-based skip list implementation of the Fraser algorithm
 *   "Practical Lock Freedom", K. Fraser,
 *   PhD dissertation, September 2003
 *   Cambridge University Technical Report UCAM-CL-TR-579
 *
 * Copyright (c) 2009-2010.
 *
 * fraser.c is part of Synchrobench
 *
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <iostream>
#include "../lotan-shavit/fraser.h"

extern ALIGNED(64) uint8_t levelmax[64];

int is_marked(uintptr_t i) { return (int)(i & (uintptr_t)0x01); }

uintptr_t set_mark(uintptr_t i) { return (i | (uintptr_t)0x01); }

void fraser_search(sl_intset_t *set, slkey_t key, val_t val, sl_node_t **left_list,
                   sl_node_t **right_list) {
  int i;
  sl_node_t *left, *left_next, *right, *right_next;
  
  //printf("levelmax = %d\n", *levelmax);
retry:
  left = set->head;
  for (i = *levelmax - 1; i >= 0; i--) {
    left_next = left->next[i];
    if (is_marked((uintptr_t)left_next)) {
      goto retry;
    }
      
    /* Find unmarked node pair at this level */
    for (right = left_next;; right = right_next) {
      /* Skip a sequence of marked nodes */
      while (1) {
        right_next = right->next[i];
        if (!is_marked((uintptr_t)right_next))
          break;
        right = (sl_node_t *)unset_mark((uintptr_t)right_next);
      }
      if (right->key >= key) {
        if (right->key > key)
          break;
        else if (!val || (right->key == key && right->val >= val))
          break;
      }
        
      left = right;
      left_next = right_next;
    }
    /* Ensure left and right nodes are adjacent */
    if ((left_next != right) &&
        (!ATOMIC_CAS_MB(&left->next[i], left_next, right))) {
        goto retry;
    }
        
    if (left_list != NULL)
      left_list[i] = left;
    if (right_list != NULL)
      right_list[i] = right;
    //else printf("right_list is null\n");
  }
}

int fraser_find(sl_intset_t *set, slkey_t key, val_t *val) {
  sl_node_t *succs[*levelmax];
  int result;

  //succs = (sl_node_t **)malloc(*levelmax * sizeof(sl_node_t *));
  //succs = (sl_node_t **)ssalloc(*levelmax * sizeof(sl_node_t *));
  fraser_search(set, key, 0, NULL, succs);
  result = (succs[0]->key == key && !succs[0]->deleted);
  *val = succs[0]->val; // garbage if result = 0
  //free(succs);
  //ssfree(succs);
  return result;
}

void mark_node_ptrs(sl_node_t *n) {
  int i;
  sl_node_t *n_next;

  for (i = n->toplevel - 1; i >= 0; i--) {
    do {
      n_next = n->next[i];
      if (is_marked((uintptr_t)n_next)) {
        break;
      }
    } while (!ATOMIC_CAS_MB(&n->next[i], n_next, set_mark((uintptr_t)n_next)));
  }
}

int fraser_remove(sl_intset_t *set, slkey_t key, val_t *val, int remove_succ) {
  sl_node_t *succs[*levelmax];
  int result;

  //succs = (sl_node_t **)malloc(*levelmax * sizeof(sl_node_t *));
  //succs = (sl_node_t **)ssalloc(*levelmax * sizeof(sl_node_t *));
  fraser_search(set, key, 0, NULL, succs);

  if (remove_succ) {
    result = (succs[0]->next[0] != NULL); // Don't remove tail
    key = succs[0]->key;
    *val = succs[0]->val;
  } else {
    result = (succs[0]->key == key);
    *val = succs[0]->val;
  }

  if (result == 0)
    goto end;
  /* 1. Node is logically deleted when the deleted field is not 0 */
  if (succs[0]->deleted) {
    result = 0;
    goto end;
  }

  if (ATOMIC_FETCH_AND_INC_FULL(&succs[0]->deleted) == 0) {
    /* 2. Mark forward pointers, then search will remove the node */
    mark_node_ptrs(succs[0]);

    /* MEM_BARRIER; */

    fraser_search(set, key, 0, NULL, NULL);
  } else {
    result = 0;
  }
end:
  //free(succs);
  //ssfree(succs);

  return result;
}

sl_node_t* alloc_node(slkey_t key, val_t val, int toplevel) {
  sl_node_t *node;
  node = (sl_node_t *)malloc(sizeof(sl_node_t) + toplevel * sizeof(sl_node_t *));
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->key = key;
  node->val = val;
  node->toplevel = toplevel;
  node->deleted = 0;

  MEM_BARRIER;

  return node;
}

int fraser_insert(sl_intset_t *set, slkey_t key, val_t val) {
  //sl_node_t *nnew, *new_next, *pred, *succ, **succs, **preds;
  sl_node_t *nnew, *new_next, *pred, *succ, *succs[*levelmax], *preds[*levelmax];
  int i;
  int result = 0;

  nnew = alloc_node(key, val, get_rand_level());
  //nnew = sl_new_simple_node_val(key, val, get_rand_level(), 0);
  // preds = (sl_node_t **)malloc(*levelmax * sizeof(sl_node_t *));
  // succs = (sl_node_t **)malloc(*levelmax * sizeof(sl_node_t *));
  // preds = (sl_node_t **)ssalloc(*levelmax * sizeof(sl_node_t *));
  // succs = (sl_node_t **)ssalloc(*levelmax * sizeof(sl_node_t *));
  // sl_node_t *preds[*levelmax];
  // sl_node_t *succs[*levelmax];

 retry:
  fraser_search(set, key, val, preds, succs);
  /* Update the value field of an existing node */
  if (succs[0]->key == key && succs[0]->val == val) { /* Value already in list */
    if (succs[0]->deleted) {  /* Value is deleted: remove it and retry */
      mark_node_ptrs(succs[0]);
      goto retry;
    }
    result = 0;
    //sl_delete_node(nnew);
    free(nnew);
    goto end;
  }

  for (i = 0; i < nnew->toplevel; i++) {
    nnew->next[i] = succs[i];
  }

  MEM_BARRIER;

  /* Node is visible once inserted at lowest level */
  if (!ATOMIC_CAS_MB(&preds[0]->next[0], succs[0], nnew)) {
    goto retry;
  }

  for (i = 1; i < nnew->toplevel; i++) {
    while (1) {
      pred = preds[i];
      succ = succs[i];
      /* Update the forward pointer if it is stale */
      new_next = nnew->next[i];
      if (is_marked((uintptr_t)new_next)) {
        goto success;
      }
      if ((new_next != succ) &&
          (!ATOMIC_CAS_MB(&nnew->next[i], unset_mark((uintptr_t)new_next),
                          succ)))
        break; /* Give up if pointer is marked */
      /* Check for old reference to a k node */
      if (succ->key == key && succ->val == val)
        succ = (sl_node_t *)unset_mark((uintptr_t)succ->next);
      /* We retry the search if the CAS fails */
      if (ATOMIC_CAS_MB(&pred->next[i], succ, nnew))
        break;

      /* MEM_BARRIER; */
      fraser_search(set, key, val, preds, succs);
    }
  }

 success:
  result = 1;
 end:
  //free(preds);
  //free(succs);
  // ssfree(preds);
  // ssfree(succs);

  return result;
}
