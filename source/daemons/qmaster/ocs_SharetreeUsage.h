#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief TODO describe this file
 */

#include <cstdint>

namespace ocs {
   /** Dirty-set of user and project objects awaiting work (event emission and/or
    *  spool to the persistent store), introduced by CS-1239.
    *
    *  Two independent dirty axes, both backed by FIFO + dedup:
    *
    *    - SPOOL dirty: worker threads call mark_user_spool_dirty /
    *      mark_project_spool_dirty after summing a finished job's usage into
    *      the in-memory user/project objects. The TET share-tree spool
    *      handler calls spool_budget once per tick to spool a budget-bounded
    *      slice. Shutdown calls spool_all to drain whatever is left before
    *      the spool backend is torn down.
    *
    *    - EVENT dirty: worker threads and the TET decay handler call
    *      mark_user_event_dirty / mark_project_event_dirty for each user /
    *      project whose UU_/PR_/UPP_ usage just changed. The TET share-tree
    *      tick handler drains both FIFOs inside one event-master transaction
    *      (sge_set_commit_required / sge_commit) and batches the resulting
    *      sgeE_USER_MOD / sgeE_PROJECT_MOD events with the sgeE_NEW_SHARETREE
    *      republish, restoring the pre-CS-1239 invariant that mirror clients
    *      see the three event types as one atomic snapshot. Shutdown calls
    *      emit_events_all to emit whatever is left before the event master
    *      is torn down.
    *
    *  The share-tree-dirty bool is independent of the user/project FIFOs:
    *  it is set when the master share tree itself is now stale (any finish,
    *  any decay pass) and consumed when the tick handler republishes.
    *
    *  All methods must be called under LOCK_GLOBAL. The FIFOs and the bool
    *  are implicitly serialised through that lock; no second-level mutex.
    */
   class SharetreeUsage {
   public:
      static void mark_user_spool_dirty(const char *name);

      static void mark_project_spool_dirty(const char *name);

      static int spool_budget(int budget_ms, uint64_t gdi_session);

      static void spool_all(uint64_t gdi_session);

      static void mark_user_event_dirty(const char *name);

      static void mark_project_event_dirty(const char *name);

      static bool has_event_dirty();

      static void emit_dirty_user_events(uint64_t gdi_session);

      static void emit_dirty_project_events(uint64_t gdi_session);

      static void emit_events_all(uint64_t gdi_session);

      static void mark_share_tree_dirty();

      static bool consume_share_tree_dirty();
   };
}

void
sge_reschedule_sharetree_tick();

void
sge_reschedule_sharetree_spool();
