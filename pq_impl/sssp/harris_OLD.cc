/*
 * File: harris.cc
 * Original File: harris.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch> - Original (harris.c)
 * 		Description:
 *   	Lock-free linkedlist implementation of Harris' algorithm
 *   	"A Pragmatic Implementation of Non-Blocking Linked Lists" 
 * 		T. Harris, p. 300-314, DISC 2001.
 *
 * Copyright (c) 2009-2010.
 *
 * harris.c is part of Synchrobench
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * 
 */

#include <iostream>
#include "../harris_ll/harris.h"

int cur_offset = 0;
node__t *last_deleted = nullptr;

node__t *new_node(k_t key, val__t val, int idx, int zone, node__t *next) {
	node__t *node = (node__t *)malloc(sizeof(node__t));
	if (node == NULL) {
		perror("malloc");
		exit(1);
	}
	node->key = key;
	node->val = val;
	node->idx = idx;
	node->zone = zone;
	node->next = next;

	return node;
}

intset_t *set_new() {
  intset_t *set;
  node__t *min, *max;
	
  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node(KEY_MAX, EMPTY, EMPTY, EMPTY, nullptr);
  min = new_node(KEY_MIN, EMPTY, EMPTY, EMPTY, max);
  set->head = min;
  set->tail = max;
  set->max_offset = 32; // todo: make this a parameter

  return set;
}

void set_destroy(intset_t *set) {
  node__t *node, *next;

  node = set->head;
  while (node != NULL) {
	if (is_marked_reference(node)) {
		node__t* tmp = (node__t*)get_unmarked_reference(node);
		next = tmp->next;
		free(tmp);
		node = next;
	} else {
		next = node->next;
    	free(node);
    	node = next;
	}
  }
  free(set);
}

long set_size(intset_t *set) {
  long size = 0;
  node__t *node;

  int num_logdel = 0;
  int num_moved = 0;

  /* We have at least 2 elements */
  node = set->head->next;
  while ((node__t*)get_unmarked_reference(node) != set->tail) {
	if (is_marked_reference(node)) {
		
		node__t* tmp = (node__t*)get_unmarked_reference(node);
		if (is_moving_ref(node)) {
			num_moved++;
		} else {
			num_logdel++;
		}
		node = tmp->next;
	} else {
		size++;
    	node = node->next;
	}
  }
  return size;
}

long long set_keysum(intset_t *set) {
  long long sum = 0;
  node__t *node;

  int cnt = 0;

  /* We have at least 2 elements */
  node = set->head->next;
  while ((node__t*)get_unmarked_reference(node) != set->tail) {
	if (is_marked_reference(node)) {
		node__t* tmp = (node__t*)get_unmarked_reference(node);
		node = tmp->next;
	} else {
		cnt++;
		sum += node->key;
    	node = node->next;
	}
  }
  return sum;
}

int set_validate(intset_t *set, int largest_per_idx[96]) {
  node__t *node;
  long long cur;
  int prev_key = -2;
  int num_incorrect = 0;

  /* We have at least 2 elements */
  node = set->head->next;
  while ((node__t*)get_unmarked_reference(node) != set->tail) {
	if (is_marked_reference(node)) {
		node__t* tmp = (node__t*)get_unmarked_reference(node);
		node = tmp->next;
	} else {
		cur = node->key;
		largest_per_idx[node->zone * 24 + node->idx] = cur;
		if (prev_key > cur) {
			std::cout << "INCORRECT ORDER! prev_key = " << prev_key << ", cur_key = " << cur << "\n";
			num_incorrect++;
		}
		prev_key = cur;
    	node = node->next;
	}
  }
  return num_incorrect;
}

void print_set(intset_t *set) {
  node__t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  std::cout << "\n";
  while ((node__t*)get_unmarked_reference(node) != set->tail) {
	if (is_marked_reference(node)) {
		node__t* tmp = (node__t*)get_unmarked_reference(node);
		if (is_logdel_ref(node)) {
			std::cout << "(" << tmp->idx <<  ") " << tmp->key << "[LOG-DEL]\n";
		}
		if (is_moving_ref(node)) {
			std::cout << "(" << tmp->idx <<  ") " << tmp->key << "[MOVING]\n";
		}
		node = tmp->next;
	} else {
		std::cout << "(" << node->idx <<  ") " << node->key << "\n";
		node = node->next;
	}
  }
  std::cout << "\n\n";
}


/* --------------------------------------------------- */
/* --------------------------------------------------- */
/*                      METHODS                        */
/* --------------------------------------------------- */
/* --------------------------------------------------- */


node__t *harris_search_idx(intset_t *set, int idx, int zone, node__t **left_node) {
	node__t *left_node_next, *right_node, *cur_left_node, *cur_left_node_next;
	left_node_next = set->head;
	
 search_again:
	do {
		node__t *x = set->head;
		node__t *x_next = x->next;
		
		/* 1. Find left_node and right_node */
		do {
			if (!is_moving_ref(x_next)) {
				cur_left_node = x;
				cur_left_node_next = (node__t *)get_notlogdel_ref(x_next); // in case x_next as logically deleted
			}
			x = (node__t *) get_unmarked_reference(x_next);
			if (!x->next) break;
			x_next = x->next;

			if (!is_moving_ref(x_next) && x->idx == idx && x->zone == zone) {
				(*left_node) = cur_left_node;
				left_node_next = cur_left_node_next;
				right_node = x;
			}
		} while (1);

		if (!right_node) {
			return nullptr;
		}
		
		/* 2. Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if (right_node->next && is_moving_ref(right_node->next))
				goto search_again;
			else return right_node;
		}
		
		/* 3. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if (right_node->next && is_moving_ref(right_node->next))
				goto search_again;
			else return right_node;
		}
	} while (1);
}

node__t *opt_harris_search_idx(intset_t *set, int idx, int zone, node__t **left_node) {
	node__t *left_node_next, *right_node, *cur_left_node, *cur_left_node_next;
	left_node_next = set->head;
	
 search_again:
	do {
		// node__t *x = set->head;
		// node__t *x_next = set->head->next;
		node__t *x;
		if (last_deleted == nullptr) {
			x = set->head;
		} else {
			x = last_deleted;
		}
		node__t *x_next = x->next;
		
		/* 1. Find left_node and right_node */
		do {
			if (!is_moving_ref(x_next)) {
				cur_left_node = x;
				cur_left_node_next = (node__t *)get_notlogdel_ref(x_next); // in case x_next as logically deleted
			}
			x = (node__t *) get_unmarked_reference(x_next);
			if (!x->next) break;
			x_next = x->next;

			if (!is_moving_ref(x_next) && x->idx == idx && x->zone == zone) { //! is this where I want to include prev_marked ??? - do I need it at all ???
				(*left_node) = cur_left_node;
				left_node_next = cur_left_node_next;
				right_node = x;
			}
		} while (1);

		if (!right_node) {
			return nullptr;
		}
		
		/* 2. Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if (right_node->next && is_moving_ref(right_node->next))
				goto search_again;
			else return right_node;
		}
		
		/* 3. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if (right_node->next && is_moving_ref(right_node->next))
				goto search_again;
			else return right_node;
		}
	} while (1);
}

node__t *harris_search(intset_t *set, k_t key, node__t **left_node) { // left_node init to point to head
	node__t *left_node_next, *right_node;
	
 search_again:
	do {
		bool prev_logdel; // to traverse past deleted nodes

		node__t *x = set->head;
		node__t *x_next = x->next;

		do {
			if (!is_moving_ref(x_next)) {
				*left_node = x;
				left_node_next = (node__t *)get_notlogdel_ref(x_next); // might be marked as logically deleted
			}
			x = (node__t *)get_unmarked_reference(x_next);
			if (x == set->tail) break;
			prev_logdel = is_logdel_ref(x_next);
			x_next = x->next;
		} while (x->key < key || is_moving_ref(x_next) || prev_logdel);

		right_node = x;
		
		/* 2. Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if ((right_node->next && is_moving_ref(right_node->next)) || is_logdel_ref((*left_node)->next)) // latter condition makes sure right node has not since been deleted by del-min op
				goto search_again;
			else return right_node;
		}
		
		/* 3. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if ((right_node->next && is_moving_ref(right_node->next)) || is_logdel_ref((*left_node)->next))
				goto search_again;
			else return right_node;
		}
	} while (1);
}

void harris_search_physdel(intset_t *set, node__t* search_node) { // left_node init to point to head
	node__t *left_node_next, *left_node, *right_node;
	
 search_again:
	do {
		bool prev_logdel; // to traverse past deleted nodes
		bool found = false;

		node__t *x = set->head;
		node__t *x_next = x->next;

		do {
			if (!is_moving_ref(x_next)) {
				left_node = x;
				left_node_next = (node__t *)get_notlogdel_ref(x_next); // might be marked as logically deleted
			}
			x = (node__t *)get_unmarked_reference(x_next);

			if (x == set->tail) break;
			if (x == search_node) found = true;
			
			prev_logdel = is_logdel_ref(x_next);
			x_next = x->next;
		} while (!found || is_moving_ref(x_next) || prev_logdel);

		if (!found) return; // has been deleted by another thread

		right_node = x;
		
		/* 2. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&left_node->next, left_node_next, right_node)) 
			return;
	} while (1);
}

node__t *opt_harris_search(intset_t *set, k_t key, node__t **left_node) { // left_node init to point to head
	node__t *left_node_next, *right_node;
	
 search_again:
	do {
		bool prev_logdel; // to traverse past deleted nodes

		// node__t *x = set->head;
		// node__t *x_next = set->head->next;
		node__t *x;
		if (last_deleted == nullptr) {
			x = set->head;
		} else {
			x = last_deleted;
		}
		node__t *x_next = x->next;

		do {
			if (!is_moving_ref(x_next)) {
				*left_node = x;
				left_node_next = (node__t *)get_notlogdel_ref(x_next); // might be marked as logically deleted
			}
			x = (node__t *)get_unmarked_reference(x_next);
			if (x == set->tail) break;
			prev_logdel = is_logdel_ref(x_next);
			x_next = x->next;
		} while (x->key < key || is_moving_ref(x_next) || prev_logdel);

		right_node = x;
		
		/* 2. Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if ((right_node->next && is_moving_ref(right_node->next)) || is_logdel_ref((*left_node)->next)) // latter condition makes sure right node has not since been deleted by del-min op
				goto search_again;
			else return right_node;
		}
		
		/* 3. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if ((right_node->next && is_moving_ref(right_node->next)) || is_logdel_ref((*left_node)->next))
				goto search_again;
			else return right_node;
		}
	} while (1);
}

/*
 * harris_find inserts a new node with the given priority key and value val in the list
 * (if absent) or does nothing (if the key-value pair is already present).
 */
int harris_insert(intset_t *set, int idx, int zone, k_t key, val__t val) {
	node__t *newnode, *right_node, *left_node;
	left_node = set->head;
	
	do {
		right_node = harris_search(set, key, &left_node);
		//right_node = opt_harris_search(set, key, &left_node);
		if (right_node->key == key && right_node->val == val) // optimization for the SSSP alg
			return 0;
			
		newnode = new_node(key, val, idx, zone, right_node);
		/* mem-bar between node creation and insertion */
		MEMORY_BARRIER;
		if (__sync_bool_compare_and_swap(&left_node->next, right_node, newnode)) {
			return 1;
		}
		free(newnode);
	} while(1);
}

val__t harris_delete_idx(intset_t *set, int idx, int zone, k_t* key) {
	node__t *right_node, *right_node_next, *left_node;// *left_node_next, *old_right_node;
	left_node = set->head;
	
	do {
		right_node = harris_search_idx(set, idx, zone, &left_node);

		if (right_node == set->tail) {
			*key = EMPTY;
			return EMPTY;
		}
		assert(right_node->idx == idx);

		right_node_next = right_node->next;

		if (!is_marked_reference(right_node_next)) {
			if (__sync_bool_compare_and_swap(&right_node->next, right_node_next, get_moving_ref(right_node_next))) {
				// indicates to concurrent insertions that we are moving this node
				break;
			}
		}
	} while(1);

	*key = right_node->key;
	val__t ret_val = right_node->val;

	if (!__sync_bool_compare_and_swap(&left_node->next, right_node, right_node_next)) {
		harris_search_physdel(set, right_node);
	}
	return ret_val;
}

val__t linden_delete_min(intset_t *set, k_t* del_key, int* del_idx, int* del_zone) {
	node__t *x, *x_next, *new_head, *obs_head;

	val__t ret_val = EMPTY;
	int offset = 0;

	x = set->head;
	obs_head = set->head->next;
	
	/* Find left_node and right_node */
	do {
		offset++;
		x_next = x->next;

		if (get_notlogdel_ref(x_next) == set->tail) {
			*del_key = EMPTY;
			return ret_val; 
		}

		if (is_logdel_ref(x_next))
			continue;

		x_next = __sync_fetch_and_or(&x->next, 1); // lin pt.
	} while ((x = (node__t *)get_notlogdel_ref(x_next)) && is_logdel_ref(x_next));
	new_head = x;

	*del_key = x->key;
	*del_idx = x->idx;
	*del_zone = x->zone;
	ret_val = x->val;
	
	// check if we should perform physical deletion
	if (offset >= set->max_offset) {
		set->head->next = (node__t *)get_logdel_ref(new_head);
		/* -- uncomment if more than one thread performs del-min concurrently
		if (set->head->next == obs_head) {
			__sync_bool_compare_and_swap(&set->head->next, obs_head, get_logdel_ref(new_head))); // either we succeed, or someone else does
		}
		*/
	}
	return ret_val;
}

val__t opt_linden_delete_min(intset_t *set, k_t* del_key, int* del_idx, int* del_zone) {
	node__t *x, *x_next, *new_head, *obs_head;
	val__t ret_val = EMPTY;

	obs_head = set->head->next;
	cur_offset += 1;

	if (last_deleted == nullptr) {
		x = set->head;
	} else {
		x = last_deleted;
	}
	
	x_next = __sync_fetch_and_or(&x->next, 1);
	assert(!is_logdel_ref(x_next));

	new_head = x_next;
	last_deleted = x_next;

	*del_key = x_next->key;
	*del_idx = x_next->idx;
	*del_zone = x_next->zone;
	ret_val = x_next->val;
	
	// check if we should perform physical deletion
	if (cur_offset >= set->max_offset) {
		// try to update head node
		if (set->head->next == obs_head) { // optimization
			if (__sync_bool_compare_and_swap(&set->head->next, obs_head,
								get_logdel_ref(new_head))) {
				cur_offset = 1;
				// todo: mark for recycling
			}
		}
	}
	return ret_val;
}