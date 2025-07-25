/*
 * File:
 *   harris.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Lock-free linkedlist implementation of Harris' algorithm
 *   "A Pragmatic Implementation of Non-Blocking Linked Lists" 
 *   T. Harris, p. 300-314, DISC 2001.
 *
 * Copyright (c) 2009-2010.
 *
 * harris.c is part of Synchrobench
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

#include "../harris_ll_4leaders/harris_4leaders.h"

node_multi_t* last_deleted_0 = nullptr;
node_multi_t* last_deleted_1 = nullptr;
node_multi_t* last_deleted_2 = nullptr;
node_multi_t* last_deleted_3 = nullptr;

int cur_offset_0 = 0;
int cur_offset_1 = 0;
int cur_offset_2 = 0;
int cur_offset_3 = 0;

node_multi_t** get_last_deleted_multi(int zone) {
	switch(zone) {
		case 0: return &last_deleted_0; break;
		case 1: return &last_deleted_1; break;
		case 2: return &last_deleted_2; break;
		case 3: return &last_deleted_3; break;
		default: return nullptr; break;
	}
}

int* get_curr_offset_multi(int zone) {
	switch(zone) {
		case 0: return &cur_offset_0; break;
		case 1: return &cur_offset_1; break;
		case 2: return &cur_offset_2; break;
		case 3: return &cur_offset_3; break;
		default: return nullptr; break;
	}
}


node_multi_t *new_node_multi(k_t key, val__t val, int idx, int zone, node_multi_t *next) {
	node_multi_t *node = (node_multi_t *)malloc(sizeof(node_multi_t));
	if (node == NULL) {
		perror("malloc");
		exit(1);
	}

	//node->inserting = 1;
	node->key = key;
	node->val = val;
	node->idx = idx;
	node->zone = zone;
	node->next = next;

	return node;
}

intset_multi_t *set_new_multi() {
  intset_multi_t *set;
  node_multi_t *min, *max;
	
  if ((set = (intset_multi_t *)malloc(sizeof(intset_multi_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node_multi(KEY_MAX, EMPTY, EMPTY, EMPTY, nullptr);
  min = new_node_multi(KEY_MIN, EMPTY, EMPTY, EMPTY, max);
  set->head = min;
  set->tail = max;
  set->max_offset = 32; // todo: make this a parameter - perform experiment to see

  return set;
}

void set_destroy_multi(intset_multi_t *set) {
  node_multi_t *node, *next;

  node = set->head;
  while (node != NULL) {
	if (is_marked_reference_multi(node)) {
		node_multi_t* tmp = (node_multi_t*)get_unmarked_reference_multi(node);
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

long set_size_multi(intset_multi_t *set) {
  long size = 0;
  node_multi_t *node;

  int num_logdel = 0;
  int num_moved = 0;

  /* We have at least 2 elements */
  node = set->head->next;
  while ((node_multi_t*)get_unmarked_reference_multi(node) != set->tail) {
	if (is_marked_reference_multi(node)) {
		
		node_multi_t* tmp = (node_multi_t*)get_unmarked_reference_multi(node);
		if (is_moving_ref_multi(node)) {
			num_moved++;
			//std::cout << "\t(" << tmp->idx << ") " << tmp->key << " (moving)\n";
		} else {
			num_logdel++;
			//std::cout << "\t(" << tmp->idx << ") " << tmp->key << " (log_del)\n";
		}
		node = tmp->next;
	} else {
		size++;
    	node = node->next;
	}
  }
//   std::cout << "\nnum logically deleted nodes: " << num_logdel << "\n";
//   std::cout << "num moving nodes: " << num_moved << "\n\n";
  return size;
}

long long set_keysum_multi(intset_multi_t *set) {
  long long sum = 0;
  node_multi_t *node;

  int cnt = 0;

  /* We have at least 2 elements */
  node = set->head->next;
  while ((node_multi_t*)get_unmarked_reference_multi(node) != set->tail) {
	if (is_marked_reference_multi(node)) {
		node_multi_t* tmp = (node_multi_t*)get_unmarked_reference_multi(node);
		node = tmp->next;
	} else {
		cnt++;
		sum += node->key;
    	node = node->next;
	}
  }
  return sum;
}

int set_validate_multi(intset_multi_t *set, int largest_per_idx[96]) {
  node_multi_t *node;
  long long cur;
  int prev_key = -2;
  int num_incorrect = 0;

  /* We have at least 2 elements */
  node = set->head->next;
  while ((node_multi_t*)get_unmarked_reference_multi(node) != set->tail) {
	if (is_marked_reference_multi(node)) {
		node_multi_t* tmp = (node_multi_t*)get_unmarked_reference_multi(node);
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

void print_set_multi(intset_multi_t *set) {
  node_multi_t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  std::cout << "\n";
  while ((node_multi_t*)get_unmarked_reference_multi(node) != set->tail) {
	if (is_marked_reference_multi(node)) {
		node_multi_t* tmp = (node_multi_t*)get_unmarked_reference_multi(node);
		if (is_moving_ref_multi(node)) {
			std::cout << "(" << tmp->idx << ") " << tmp->key << " (moving)\n";
		} else {
			std::cout << "(" << tmp->idx << ") " << tmp->key << " (log_del)\n";
		}
		node = tmp->next;
	} else {
		std::cout << "(" << node->idx << ") " << node->key << "\n";
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



node_multi_t *harris_search_idx_multi(intset_multi_t *set, int idx, int zone, node_multi_t **left_node) {
	node_multi_t *left_node_next, *right_node, *cur_left_node, *cur_left_node_next;
	left_node_next = set->head;
	
search_again:
	do {
		node_multi_t *x = set->head;
		node_multi_t *x_next = set->head->next;
		
		/* 1. Find left_node and right_node */
		do {
			if (!is_moving_ref_multi(x_next)) {
				cur_left_node = x;
				cur_left_node_next = (node_multi_t *)get_notlogdel_ref_multi(x_next); // in case x_next as logically deleted
			}
			x = (node_multi_t *) get_unmarked_reference_multi(x_next);
			if (!x->next) break;
			x_next = x->next;

			if (!is_moving_ref_multi(x_next) && x->idx == idx && x->zone == zone) { //! is this where I want to include prev_marked ??? - do I need it at all ???
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
			if (right_node->next && is_moving_ref_multi(right_node->next))
				goto search_again;
			else return right_node;
		}
		
		/* 3. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if (right_node->next && is_moving_ref_multi(right_node->next))
				goto search_again;
			else return right_node;
		}
	} while (1);
}

node_multi_t *harris_search_multi(intset_multi_t *set, k_t key, node_multi_t **left_node) { // left_node init to point to head
	node_multi_t *left_node_next, *right_node;
	
search_again:
	do {
		bool prev_logdel; // to traverse past deleted nodes

		node_multi_t *x = set->head;
		node_multi_t *x_next = set->head->next;

		do {
			if (!is_moving_ref_multi(x_next)) {
				*left_node = x;
				left_node_next = (node_multi_t *)get_notlogdel_ref_multi(x_next); // might be marked as logically deleted
			}
			x = (node_multi_t *)get_unmarked_reference_multi(x_next);
			if (x == set->tail) break;
			prev_logdel = is_logdel_ref_multi(x_next);
			x_next = x->next;
		} while (x->key < key || is_moving_ref_multi(x_next) || prev_logdel);

		right_node = x;
		
		/* 2. Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if ((right_node->next && is_moving_ref_multi(right_node->next)) || is_logdel_ref_multi((*left_node)->next)) // latter condition makes sure right node has not since been deleted by del-min op
				goto search_again;
			else return right_node;
		}
		
		/* 3. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if ((right_node->next && is_moving_ref_multi(right_node->next)) || is_logdel_ref_multi((*left_node)->next))
				goto search_again;
			else return right_node;
		}
	} while (1);
}

node_multi_t *opt_harris_search_idx_multi(intset_multi_t *set, int idx, int zone, node_multi_t **left_node) {
	node_multi_t *left_node_next, *right_node, *cur_left_node, *cur_left_node_next;
	left_node_next = set->head;
	
 search_again:
	do {
		node_multi_t** last_del_ptr = get_last_deleted_multi(zone);
		node_multi_t *x;
		if (*last_del_ptr == nullptr) {
			x = set->head;
		} else {
			x = *last_del_ptr;
		}
		node_multi_t *x_next = x->next;
		
		/* 1. Find left_node and right_node */
		do {
			if (!is_moving_ref_multi(x_next)) {
				cur_left_node = x;
				cur_left_node_next = (node_multi_t *)get_notlogdel_ref_multi(x_next); // in case x_next as logically deleted
			}
			x = (node_multi_t *) get_unmarked_reference_multi(x_next);
			if (!x->next) break;
			x_next = x->next;

			if (!is_moving_ref_multi(x_next) && x->idx == idx && x->zone == zone) { //! is this where I want to include prev_marked ??? - do I need it at all ???
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
			if (right_node->next && is_moving_ref_multi(right_node->next))
				goto search_again;
			else return right_node;
		}
		
		/* 3. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if (right_node->next && is_moving_ref_multi(right_node->next))
				goto search_again;
			else return right_node;
		}
	} while (1);
}

node_multi_t* opt_harris_search_multi(intset_multi_t *set, k_t key, int zone, node_multi_t **left_node) { // left_node init to point to head
	node_multi_t *left_node_next, *right_node;
	
 search_again:
	do {
		bool prev_logdel; // to traverse past deleted nodes

		node_multi_t** last_del_ptr = get_last_deleted_multi(zone);
		node_multi_t *x;
		if (*last_del_ptr == nullptr) {
			x = set->head;
		} else {
			x = *last_del_ptr;
		}
		node_multi_t *x_next = x->next;

		do {
			if (!is_moving_ref_multi(x_next)) {
				*left_node = x;
				left_node_next = (node_multi_t *)get_notlogdel_ref_multi(x_next); // might be marked as logically deleted
			}
			x = (node_multi_t *)get_unmarked_reference_multi(x_next);
			if (x == set->tail) break;
			prev_logdel = is_logdel_ref_multi(x_next);
			x_next = x->next;
		} while (x->key < key || is_moving_ref_multi(x_next) || prev_logdel);

		right_node = x;
		
		/* 2. Check that nodes are adjacent */
		if (left_node_next == right_node) {
			if ((right_node->next && is_moving_ref_multi(right_node->next)) || is_logdel_ref_multi((*left_node)->next)) // latter condition makes sure right node has not since been deleted by del-min op
				goto search_again;
			else return right_node;
		}
		
		/* 3. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&(*left_node)->next, 
						  left_node_next, 
						  right_node)) {
			if ((right_node->next && is_moving_ref_multi(right_node->next)) || is_logdel_ref_multi((*left_node)->next))
				goto search_again;
			else return right_node;
		}
	} while (1);
}


/*
 * harris_find inserts a new node with the given priority key and value val in the list
 * (if absent) or does nothing (if the key-value pair is already present).
 */
int harris_insert_multi(intset_multi_t *set, int idx, int zone, k_t key, val__t val) {
	node_multi_t *newnode, *right_node, *left_node;
	left_node = set->head;
	
	do {
		
		right_node = harris_search_multi(set, key, &left_node);
		//right_node = opt_harris_search(set, key, zone, &left_node);
		
		if (right_node->key == key && right_node->val == val) //TODO: (applies to og harris as well) what if this is true, but node is logically deleted between harris_search() call, and this check? wouldn't this be incorrect?
			return 0;
			
		newnode = new_node_multi(key, val, idx, zone, right_node);
		/* mem-bar between node creation and insertion */
		MEMORY_BARRIER;
		if (__sync_bool_compare_and_swap(&left_node->next, right_node, newnode)) {
			return 1;
		}
		free(newnode);
	} while(1);
}

void harris_search_physdel_multi(intset_multi_t *set, node_multi_t* search_node) { // left_node init to point to head
	node_multi_t *left_node_next, *left_node, *right_node;
	
 search_again:
	do {
		bool prev_logdel; // to traverse past deleted nodes
		bool found = false;

		node_multi_t *x = set->head;
		node_multi_t *x_next = x->next;

		do {
			if (!is_moving_ref_multi(x_next)) {
				left_node = x;
				left_node_next = (node_multi_t *)get_notlogdel_ref_multi(x_next); // might be marked as logically deleted
			}
			x = (node_multi_t *)get_unmarked_reference_multi(x_next);

			if (x == set->tail) break;
			if (x == search_node) found = true;
			
			prev_logdel = is_logdel_ref_multi(x_next);
			x_next = x->next;
		} while (!found || is_moving_ref_multi(x_next) || prev_logdel);

		if (!found) return; // has been deleted by another thread

		right_node = x;
		
		/* 2. Remove one or more marked nodes */
		if (__sync_bool_compare_and_swap(&left_node->next, left_node_next, right_node)) 
			return;
	} while (1);
}


val__t harris_delete_idx_multi(intset_multi_t *set, int idx, int zone, k_t* key) {
	node_multi_t *right_node, *right_node_next, *left_node;// *left_node_next, *old_right_node;
	left_node = set->head;
	
	do {
		right_node = harris_search_idx_multi(set, idx, zone, &left_node);
		//right_node = opt_harris_search_idx(set, idx, zone, &left_node);

		if (right_node == set->tail) { // empty / not found
			*key = EMPTY;
			return EMPTY;
		}
		assert(right_node->idx == idx);

		right_node_next = right_node->next;

		if (!is_marked_reference_multi(right_node_next)) {
			if (__sync_bool_compare_and_swap(&right_node->next, right_node_next, get_moving_ref_multi(right_node_next))) {
				// indicates to concurrent insertions that we are moving this node
				break;
			}
		}
	} while(1);


	*key = right_node->key;
	val__t ret_val = right_node->val;

	if (!__sync_bool_compare_and_swap(&left_node->next, right_node, right_node_next)) {
		//right_node = harris_search(set, right_node->key, &left_node);
		//right_node = opt_harris_search(set, right_node->key, zone, &left_node);
		harris_search_physdel_multi(set, right_node);
	}
	return ret_val;
}


val__t linden_delete_min_multi(intset_multi_t *set, k_t* del_key, int* del_idx, int del_zone) {
	node_multi_t *x, *x_next, *new_head, *obs_head;
	new_head = nullptr;

	val__t ret_val = EMPTY;
	int offset = 0;

	x = set->head;
	obs_head = x->next;
	
	/* Find left_node and right_node */
	do {
		offset++;
		x_next = x->next;

		if (get_notlogdel_ref_multi(x_next) == set->tail) {
			*del_key = EMPTY;
			return ret_val; 
		}

		if (is_logdel_ref_multi(x_next))
			continue;

		// lin pt.
		x_next = __sync_fetch_and_or(&x->next, 1);
	} while ((x = (node_multi_t *)get_notlogdel_ref_multi(x_next)) && is_logdel_ref_multi(x_next));

	*del_key = x->key;
	*del_idx = x->idx;
	ret_val = x->val;

	if (new_head == nullptr)
		new_head = x;
	
	// check if we should perform physical deletion
	if (offset >= set->max_offset) {
		// set->head->next = (node_multi_t*)get_logdel_ref_multi(new_head);
		// try to update head node
		if (set->head->next == obs_head) { // optimization
			__sync_bool_compare_and_swap(&set->head->next, obs_head,
								(node_multi_t*)get_logdel_ref_multi(new_head));
		}
	}
	return ret_val;
}

val__t opt_linden_delete_min_multi(intset_multi_t *set, k_t* del_key, int* del_idx, int del_zone) {
	node_multi_t *x, *x_next, *new_head, *obs_head;
	node_multi_t** last_del_ptr = get_last_deleted_multi(del_zone);
	val__t ret_val = EMPTY;

	obs_head = set->head->next;
	int* cur_offset = get_curr_offset_multi(del_zone);
	*cur_offset += 1;

	if (*last_del_ptr == nullptr) {
		x = set->head;
	} else {
		x = *last_del_ptr;
	}
	
	x_next = __sync_fetch_and_or(&x->next, 1);
	assert(!is_logdel_ref_multi(x_next));

	new_head = x_next;
	*last_del_ptr = x_next;

	*del_key = x_next->key;
	*del_idx = x_next->idx;
	ret_val = x_next->val;
	
	// check if we should perform physical deletion
	if (*cur_offset >= set->max_offset) {
		set->head->next = (node_multi_t*)get_logdel_ref_multi(new_head);
		*cur_offset = 1;
		//try to update head node
		// if (set->head->next == obs_head) { // optimization
		// 	if (__sync_bool_compare_and_swap(&set->head->next, obs_head,
		// 						get_logdel_ref_multi(new_head))) {
		// 		*cur_offset = 1;
		// 		// todo: mark for recycling
		// 	}
		// }
	}
	return ret_val;
}

node_multi_t* linden_peek_min_multi(intset_multi_t *set) {
	node_multi_t *x, *x_next;
	x = set->head;
	
	do {
		x_next = x->next;
		
		if (get_notlogdel_ref_multi(x_next) == set->tail) {
			return nullptr;
		}

		if (is_logdel_ref_multi(x_next))
			continue;

	} while((x = (node_multi_t *)get_notlogdel_ref_multi(x_next)) && is_logdel_ref_multi(x_next));
	return x;
}

node_multi_t* opt_linden_peek_min_multi(intset_multi_t *set, int zone) {
	node_multi_t *x, *x_next;
	node_multi_t** last_del_ptr = get_last_deleted_multi(zone);
	
	if (*last_del_ptr == nullptr) {
		x = set->head;
	} else {
		x = *last_del_ptr;
	}
	
	x_next = x->next;
	assert(!is_logdel_ref_multi(x_next));

	return x_next;
}