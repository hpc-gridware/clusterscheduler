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
      /** Mark a user object dirty for spooling. Names are de-duplicated: a
       *  second call with the same name while the first is still queued is
       *  a no-op.
       */
      static void mark_user_spool_dirty(const char *name);

      /** Mark a project object dirty for spooling. De-duplicated. */
      static void mark_project_spool_dirty(const char *name);

      /** Drain the head of the user and project spool FIFOs to the spool
       *  backend for up to budget_ms milliseconds. Returns the residual queue
       *  size (sum of users + projects still spool-dirty). A non-zero return
       *  means the caller should reschedule sooner; zero means the next idle
       *  gap is fine.
       */
      static int spool_budget(int budget_ms, uint64_t gdi_session);

      /** Drain both spool FIFOs entirely, ignoring any time budget. Called
       *  from the qmaster shutdown sequence before the spool backend is torn
       *  down.
       */
      static void spool_all(uint64_t gdi_session);

      /** Mark a user object dirty for event emission. De-duplicated. The
       *  next share-tree tick will emit sgeE_USER_MOD for this user inside
       *  the publish transaction.
       */
      static void mark_user_event_dirty(const char *name);

      /** Mark a project object dirty for event emission. De-duplicated. */
      static void mark_project_event_dirty(const char *name);

      /** Returns true if either event FIFO has at least one entry. The
       *  share-tree tick handler uses this to decide whether to open an
       *  event-master transaction at all.
       */
      static bool has_event_dirty();

      /** Drain the user event FIFO, emitting sgeE_USER_MOD for each entry
       *  via sge_add_event. Must be called inside an open event-master
       *  transaction (sge_set_commit_required ... sge_commit) so the events
       *  ship as one package with the trailing sgeE_NEW_SHARETREE.
       */
      static void emit_dirty_user_events(uint64_t gdi_session);

      /** Drain the project event FIFO, emitting sgeE_PROJECT_MOD for each
       *  entry. Same transaction-context contract as emit_dirty_user_events.
       */
      static void emit_dirty_project_events(uint64_t gdi_session);

      /** Drain both event FIFOs entirely, emitting the queued MOD events.
       *  Called from the qmaster shutdown sequence so finish-driven events
       *  do not get dropped on the last tick interval. Caller is responsible
       *  for the surrounding transaction (or for accepting the per-event
       *  package overhead if shutdown speed beats atomicity here).
       */
      static void emit_events_all(uint64_t gdi_session);

      /** CS-1239 step 5: mark the master share tree as "needs republish".
       *  Set by the worker thread after sge_book_finished_job_usage sums a
       *  finish into UU_/PR_/UPP_ usage, and by the TET decay handler after
       *  a decay pass. Consumed (and cleared) by the TET share-tree tick
       *  handler when it next emits sgeE_NEW_SHARETREE.
       */
      static void mark_share_tree_dirty();

      /** Returns true and clears the flag atomically (under LOCK_GLOBAL).
       *  The tick handler uses this so the "saw dirty -> clear -> publish"
       *  sequence cannot lose a concurrent mark.
       */
      static bool consume_share_tree_dirty();
   };
}

/** CS-1239: re-schedule the periodic share-tree tick event with the *current*
 *  mconf_get_sharetree_tick_interval() value. Drops the pending one-time event
 *  and queues a fresh one at +5 s so a config change to STREE_TICK_INTERVAL
 *  takes effect within seconds rather than after the remaining (up to 300 s)
 *  lifetime of the already-queued event. Called from
 *  configuration_qmaster.cc::do_mod_config after merge_configuration()
 *  re-parses qmaster_params. Safe to call when no tick event is queued
 *  (delete is a no-op in that case); also safe to call from any thread
 *  (te_add_event / te_delete_one_time_event are MT-safe). */
void
sge_reschedule_sharetree_tick();

/** CS-1239: re-schedule the periodic share-tree spool event. Same
 *  rationale as sge_reschedule_sharetree_tick() but for STREE_SPOOL_INTERVAL:
 *  a config change to STREE_SPOOL_INTERVAL would otherwise only take effect
 *  after the already-queued event fires (up to 240 s away on default
 *  configs), which made tests like issue_1385 - which pin the interval and
 *  read the spool file shortly after - flake. Drops the pending event and
 *  re-queues at +5 s. Same thread-safety / no-op-if-not-queued guarantees. */
void
sge_reschedule_sharetree_spool();
