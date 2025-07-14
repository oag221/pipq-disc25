#include "lotan_shavit.h"
//#include "skiplist.h"

void lotan_shavit_ns::lotan_shavit::lotan_init() {
    //*levelmax = floor_log_2(MAX_KEY) + 2;
    *levelmax = 32;
    lotan_ds = sl_set_new();
}

long lotan_shavit_ns::lotan_shavit::debugKeySum() {
    return sl_set_keysum(lotan_ds);
}

void lotan_shavit_ns::lotan_shavit::thread_init() {
  seeds = seed_rand();
}

std::string lotan_shavit_ns::lotan_shavit::getSizeString() {
    return std::to_string(sl_set_size(lotan_ds));
}

bool lotan_shavit_ns::lotan_shavit::getValidated() {
    return true;
}

int lotan_shavit_ns::lotan_shavit::lotan_shavit_delete_min(val_t *val) {
  slkey_t key;
  return lotan_shavit_delete_min_key(&key, val);
}

int lotan_shavit_ns::lotan_shavit::lotan_shavit_delete_min_key(slkey_t *key, val_t *val) {
  sl_node_t *first;
  int result;

  first = lotan_ds->head;

  while (1) {
    do {
      first = (sl_node_t *)unset_mark((uintptr_t)first->next[0]);
    } while (first->next[0] && first->deleted);
    if (first->next[0] && ATOMIC_FETCH_AND_INC_FULL(&first->deleted) != 0) {
    } else { // todo: reverse logic to make this cleaner
      break;
    }
  }

  result = (first->next[0] != NULL);
  if (!result) {
    *key = -1;
    return 0;
  }

  *val = (first->val);
  *key = (first->key);
  mark_node_ptrs(first);

  fraser_search(lotan_ds, first->key, 0, NULL, NULL);

  return result;
}

int lotan_shavit_ns::lotan_shavit::lotan_fraser_insert(slkey_t key, val_t val) {
  return fraser_insert(lotan_ds, key, val);
//   sl_node_t *nnew, *new_next, *pred, *succ, **succs, **preds;
//   int i;
//   int result = 0;

//   nnew = sl_new_simple_node_val(key, val, get_rand_level(), 0);
//   // preds = (sl_node_t **)malloc(*levelmax * sizeof(sl_node_t *));
//   // succs = (sl_node_t **)malloc(*levelmax * sizeof(sl_node_t *));
//   preds = (sl_node_t **)ssalloc(*levelmax * sizeof(sl_node_t *));
//   succs = (sl_node_t **)ssalloc(*levelmax * sizeof(sl_node_t *));

// retry:
//   //printf("searching... levelmex = %d\n", *levelmax);
//   fraser_search(lotan_ds, key, val, preds, succs);
//   /* Update the value field of an existing node */
//   if (succs[0]->key == key &&
//       succs[0]->val == val) { /* Value already in list */
//     if (succs[0]->deleted) {  /* Value is deleted: remove it and retry */
//       mark_node_ptrs(succs[0]);
//       goto retry;
//     }
//     result = 0;
//     sl_delete_node(nnew);
//     goto end;
//   }

//   for (i = 0; i < nnew->toplevel; i++) {
//     nnew->next[i] = succs[i];
//   }

//   MEM_BARRIER;

//   /* Node is visible once inserted at lowest level */
//   if (!ATOMIC_CAS_MB(&preds[0]->next[0], succs[0], nnew)) {
//     goto retry;
//   }

//   for (i = 1; i < nnew->toplevel; i++) {
//     while (1) {
//       pred = preds[i];
//       succ = succs[i];
//       /* Update the forward pointer if it is stale */
//       new_next = nnew->next[i];
//       if (is_marked((uintptr_t)new_next)) {
//         goto success;
//       }
//       if ((new_next != succ) &&
//           (!ATOMIC_CAS_MB(&nnew->next[i], unset_mark((uintptr_t)new_next),
//                           succ)))
//         break; /* Give up if pointer is marked */
//       /* Check for old reference to a k node */
//       if (succ->key == key && succ->val == val)
//         succ = (sl_node_t *)unset_mark((uintptr_t)succ->next);
//       /* We retry the search if the CAS fails */
//       if (ATOMIC_CAS_MB(&pred->next[i], succ, nnew))
//         break;

//       /* MEM_BARRIER; */
//       fraser_search(lotan_ds, key, val, preds, succs);
//     }
//   }

// success:
//   result = 1;
// end:
//   // free(preds);
//   // free(succs);
//   ssfree(preds);
//   ssfree(succs);

//   return result;
}