/*
 * File:
 *   intset.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Skip list integer set operations
 *
 * Copyright (c) 2009-2010.
 *
 * intset.h is part of Synchrobench
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

#include "fraser.h"
#include "linden.h"

inline int sl_contains_val(sl_intset_t *set, slkey_t key, val_t *val, int) {
  return fraser_find(set, key, val);
}

inline int sl_contains(sl_intset_t *set, slkey_t key, int transactional) {
  val_t val;
  return sl_contains_val(set, key, &val, transactional);
}

inline int sl_add_val(sl_intset_t *set, slkey_t key, val_t val, int) {
  return fraser_insert(set, key, val);
}

inline int sl_add(sl_intset_t *set, slkey_t key, int transactional) {
  return sl_add_val(set, key, (val_t)key, transactional);
}

inline int sl_remove_val(sl_intset_t *set, slkey_t key, val_t *val, int) {
  return fraser_remove(set, key, val, 0);
}

inline int sl_remove(sl_intset_t *set, slkey_t key, int transactional) {
  val_t val;
  return sl_remove_val(set, key, &val, transactional);
}

inline int sl_remove_succ_val(sl_intset_t *set, slkey_t key, val_t *val, int) {
  return fraser_remove(set, key, val, 1);
}

inline int sl_remove_succ(sl_intset_t *set, slkey_t key, int transactional) {
  val_t val;
  return sl_remove_succ_val(set, key, &val, transactional);
}

// priority queue
int lotan_shavit_delete_min_key(sl_intset_t *set, slkey_t *key, val_t *val,
                                thread_data_t *d);
int lotan_shavit_delete_min(sl_intset_t *set, val_t *val, thread_data_t *d);

int spray_delete_min_key(sl_intset_t *set, slkey_t *key, val_t *val,
                         thread_data_t *d);
int spray_delete_min(sl_intset_t *set, val_t *val, thread_data_t *d);
