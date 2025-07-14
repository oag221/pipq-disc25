#pragma once

/*
 * File:
 *   skiplist.h
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Stress test of the skip list implementation.
 *
 * Copyright (c) 2009-2010.
 *
 * skiplist.h is part of Synchrobench
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

#include <assert.h>
#include <getopt.h>
//#include <atomic_ops.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "../atomic_ops/atomic_ops.h"
#include "atomic_ops_if.h"
#include "ssalloc.h"
// #include "tm.h"

#define DEFAULT_DURATION 1000
#define DEFAULT_INITIAL 1024
#define DEFAULT_NB_THREADS 1
#define DEFAULT_RANGE (2 * DEFAULT_INITIAL)
#define DEFAULT_SEED 1
#define DEFAULT_UPDATE 20
#define DEFAULT_ELASTICITY 4
#define DEFAULT_ALTERNATE 0
#define DEFAULT_ES 0
#define DEFAULT_EFFECTIVE 1

// #define XSTR(s) STR(s)
// #define STR(s) #s

extern uint8_t levelmax[64];

#define TRANSACTIONAL 4 // TODO: get rid of this

typedef unsigned long slkey_t;
typedef unsigned long val_t;
typedef intptr_t level_t;
#define KEY_MIN INT_MIN
#define KEY_MAX INT_MAX
#define DEFAULT_VAL 0

// #if defined(DO_ALIGN)
#define ALIGNED(N) __attribute__((aligned(N)))
// #else
// #define ALIGNED(N)
// #endif

// typedef
struct ALIGNED(64) sl_node_t {
  slkey_t key;
  val_t val;

  int toplevel;
  intptr_t deleted;
  sl_node_t *next[19];
} // sl_node_t;
;

struct ALIGNED(64) sl_intset_t {
  sl_node_t *head;
};

int get_rand_level();
int floor_log_2(unsigned int n);

sl_node_t *sl_new_simple_node(slkey_t key, int toplevel, int transactional);
sl_node_t *sl_new_simple_node_val(slkey_t key, val_t val, int toplevel,
                                  int transactional);
sl_node_t *sl_new_node_val(slkey_t key, val_t val, sl_node_t *next,
                           int toplevel, int transactional);
sl_node_t *sl_new_node(slkey_t key, sl_node_t *next, int toplevel,
                       int transactional);
void sl_delete_node(sl_node_t *n);

sl_intset_t *sl_set_new();
void sl_set_delete(sl_intset_t *set);
int sl_set_size(sl_intset_t *set);

long sl_set_keysum(sl_intset_t *set);

//inline long rand_range(long r); /* declared in test.c */
