#pragma once

/// Instantiating the SVPQ is revealing some flaws in the design.  The most
/// significant is that std::atomic<> of a non-simple type is going to be very
/// slow, which will harm insertions.  Is "unique key" just not reasonable in
/// SkipVector?
///
/// Another issue is that -Werror is causing compilation issues in the SVPQ
/// code.

#include "sv_lin.h"

/// Extract a key from the pq
inline int sv_lin_remove(sv_lin_t *set, slkey_t *key, val_t *val) {
  ENTRY_T entry = ENTRY_S(*key, *val);
  auto res = set->extract_min(entry);

  if (!res) {
    // TODO: If extract_min() fails, the queue may not necessarily be empty, but
    // we'll go with this for now.
    // FIXME: Wait, is it possible this is causing threads to give up before the
    // task is over?
    *key = EMPTY_S;
  } else {
    *key = entry.get_k();
    *val = entry.get_v();
  }

  return res;
}

/// Insert into the pq
inline void sv_lin_insert(sv_lin_t *set, slkey_t key, val_t val) {
  set->insert({key, val});
}
