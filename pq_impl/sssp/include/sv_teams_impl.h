#pragma once

#include "sv_teams.h"

/// Extract a key from the pq
inline int sv_teams_remove(sv_teams_t *set, slkey_t *key, val_t *val) {
  ENTRY_T entry = ENTRY_T(*key, *val);
  auto res = set->extract_min(entry);

  if (!res) {
    // TODO: If extract_min() fails, the queue may not necessarily be empty, but
    // we'll go with this for now.
    // FIXME: Wait, is it possible this is causing threads to give up before the
    // task is over?
    *key = EMPTY_T;
  } else {
    *key = entry.get_k();
    *val = entry.get_v();
  }

  return res;
}

/// Insert into the pq
inline void sv_teams_insert(sv_teams_t *set, slkey_t key, val_t val) {
  set->insert({key, val});
}
