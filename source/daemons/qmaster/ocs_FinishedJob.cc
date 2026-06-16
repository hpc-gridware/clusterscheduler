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

#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Usage.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_userprj.h"

#include "ocs_SharetreeUsage.h"
#include "ocs_FinishedJob.h"
#include "sge_persistence_qmaster.h"

namespace {
   /* Remove the debited-job-usage entry for the given job number from the
    * passed list, if present. Mirrors the "UPU_old_usage_list == nullptr"
    * signal in the ORT_update_user_usage / _project_usage handlers, which is
    * how the order-driven path tells qmaster to drop the entry: with the
    * worker-thread path the qmaster IS the one doing the modification, so
    * the drop is direct. */
   void drop_debited_job(lListElem *userprj, int upu_field, uint32_t job_number) {
      lList *upu_list = lGetListRW(userprj, upu_field);
      if (upu_list == nullptr) {
         return;
      }
      lListElem *upu = lGetElemUlongRW(upu_list, UPU_job_number, job_number);
      if (upu != nullptr) {
         lRemoveElem(upu_list, &upu);
      }
   }

   /* Bump UU_/PR_version so consumers that key off it see a change.
    *
    * Do NOT bump UU_/PR_usage_seqno: that field is the periodic decay
    * handler's idempotency stamp - decay_userprj_usage skips when
    * seqno == stored_seqno. The handler's tet_decay_run counter also
    * starts at 0 and ++s by 1, so a booking bump would silently match a
    * tick and the decay would be a no-op. (That collision produced
    * u0 == u1 bit-exact in the sharetree_decay_periodic test.) */
   void bump_change_markers(lListElem *userprj, int version_field) {
      lAddUlong(userprj, version_field, 1);
   }
}

int
sge_book_finished_job_usage(lListElem *jep, lListElem *jatep, monitoring_t * /* monitor */, uint64_t gdi_session) {
   if (jep == nullptr || jatep == nullptr) {
      return -1;
   }

   /* Idempotency guard: skip if this ja_task has already been booked.
    * The "finished_jobs" entry in JAT_scaled_usage_list is set below as
    * part of the booking; if it is already there, the booking has run
    * before and we must not double-count. Mirrors the pre-CS-1239 check
    * in sge_calc_tickets ("we already handled this finished job").
    * Reschedule is safe: sge_give_jobs.cc clears JAT_scaled_usage_list on
    * COMMIT_ST_RESCHEDULED / COMMIT_ST_USER_RESCHEDULED so the next exit
    * re-seeds. The guard is defensive coverage for future paths that may
    * re-invoke this helper (e.g. CS-1908 finished-job retention). */
   if (lGetSubStr(jatep, UA_name, "finished_jobs", JAT_scaled_usage_list) != nullptr) {
      return 0;
   }

   /* CS-1239: skip the entire booking when no master share tree is configured.
    * The summed UU_/PR_/UPP_usage is consumed only by the share-tree allocator
    * and by sge_share_mon, both of which are dormant in this case. This
    * restores the pre-CS-1239 implicit gate ("no UU/PR objects -> no work";
    * see calc_usage.md) by checking the direct indicator. Trade-off: a
    * qconf -Astree after a quiet period starts the share-tree allocator from
    * zero history for one tick. Matching gate is in sge_sharetree_tick_handler.
    * Caller holds LOCK_GLOBAL write (always via sge_commit_job), so reading
    * the SHARETREE master list without an extra lock is safe. */
   if (lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SHARETREE)) == nullptr) {
      return 0;
   }

   const lList *master_user_list = *ocs::DataStore::get_master_list(SGE_TYPE_USER);
   const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);

   lListElem *user = user_list_locate(master_user_list, lGetString(jep, JB_owner));
   lListElem *project = nullptr;
   const char *project_name = lGetString(jep, JB_project);
   if (project_name != nullptr && *project_name != '\0') {
      project = prj_list_locate(master_project_list, project_name);
   }

   /* 1. catch up decay since last tick, then sum the finished job's usage
    *    into UU_/PR_/UPP_. Worker thread's sc_state_t.decay_constant is
    *    thread-local and uninitialized by default (== 0), so without this
    *    refresh decay_usage would multiply by 0 and zero everything. Same
    *    gap previously hit on the scheduler thread (fixed in
    *    sge_sharetree_tick_handler). */
   ocs::Usage::calculate_default_decay_constant(sconf_get_halftime());

   const uint64_t curr_time = sge_get_gmt64();
   lList *decay_list = ocs::Usage::get_decay_list();
   lList *usage_weight_list = sconf_get_usage_weight_list();

   /* Seed "finished_jobs"=1 onto the job's scaled_usage_list before we
    * sum it into UU_/PR_/UPP_. The pending-priority calculation in
    * sge_sort_pending_job_nodes (sgeee.cc) reads the leaf's
    * STN_usage_list "finished_jobs" entry and adds it to job_count when
    * computing per-pending-job tickets. calc_node_usage propagates the
    * project's "finished_jobs" PR_usage entry up to the leaf's
    * STN_usage_list. Without this seed the counter is never written
    * anywhere and the formula treats every leaf as "0 finished" - the
    * leaf with the fewest pending jobs wins by per-job dilution alone,
    * starving leaves with many pending (project3 in the sharetree
    * test). */
   lList *scaled = lGetListRW(jatep, JAT_scaled_usage_list);
   if (scaled == nullptr) {
      lSetList(jatep, JAT_scaled_usage_list, lCreateList("scaled_usage", UA_Type));
      scaled = lGetListRW(jatep, JAT_scaled_usage_list);
   }
   usage_list_set_double_usage(scaled, "finished_jobs", 1.0);

   /* CS-1239 (test convergence fix): the decay between the previous TET
    * tick and this finish time must apply to the historical UU_/PR_usage
    * BEFORE we sum the new job's scaled_usage onto it. Otherwise the
    * stamp jumps forward to curr_time but the historical part is "stuck
    * at the previous-tick decay state" - the next tick decays by
    * (next_tick - curr_time) which loses the (curr_time - prev_tick)
    * interval. For projects that finish often (project2 in the sharetree
    * test) this loss accumulates: combined_usage stays artificially high,
    * shr stays high relative to truth, allocator keeps picking project2,
    * test fails. seqno = (u_long)-1 tells decay_userprj_usage to update
    * the stamp but NOT update the stored seqno - the TET tick owns the
    * seqno namespace, we just want a catch-up decay tick here. */
   ocs::Usage::decay_and_sum_usage(jep, jatep, nullptr, user, project,
                                   decay_list, usage_weight_list,
                                   static_cast<u_long>(-1), curr_time);
   lFreeList(&usage_weight_list);

   const uint32_t job_number = lGetUlong(jep, JB_job_number);

   /* 2. drop the per-job debited entry the sum step just appended (the job
    *    is gone; no further deltas to compute), and bump change markers so
    *    consumers see "something happened to this object". */
   if (user != nullptr) {
      drop_debited_job(user, UU_debited_job_usage, job_number);
      bump_change_markers(user, UU_version);
   }
   if (project != nullptr) {
      drop_debited_job(project, PR_debited_job_usage, job_number);
      bump_change_markers(project, PR_version);
   }

   /* 3. mark user / project dirty for both event emission and spool. The
    *    sgeE_USER_MOD / sgeE_PROJECT_MOD events are deferred to the share-
    *    tree tick handler (sge_sharetree_tick_handler), which batches them
    *    with sgeE_NEW_SHARETREE inside one event-master transaction so the
    *    scheduler mirror sees user/project usage and tree state as one
    *    atomic snapshot (pre-CS-1239 ORT_share_tree invariant restored).
    *    Spool is deferred to the share-tree spool handler at its own
    *    cadence. */
   if (user != nullptr) {
      const char *uname = lGetString(user, UU_name);
      ocs::SharetreeUsage::mark_user_event_dirty(uname);
      ocs::SharetreeUsage::mark_user_spool_dirty(uname);
   }
   if (project != nullptr) {
      const char *pname = lGetString(project, PR_name);
      ocs::SharetreeUsage::mark_project_event_dirty(pname);
      ocs::SharetreeUsage::mark_project_spool_dirty(pname);
   }

   /* CS-1239 step 5: this finish moved usage in UU_/PR_/UPP_, which means
    * the master share tree (read by sge_share_mon and propagated to mirror
    * clients via sgeE_NEW_SHARETREE) is now stale. Flag it so the TET
    * share-tree tick handler republishes on its next tick. Cheap (one
    * bool write under the LOCK_GLOBAL we already hold). */
   if (user != nullptr || project != nullptr) {
      ocs::SharetreeUsage::mark_share_tree_dirty();
   }

   /* Bury (sge_bury_job) is performed by the caller (sge_commit_job's
    * COMMIT_ST_FINISHED_FAILED_EE case) right after this helper returns.
    * Keeping this function a pure "book the usage" routine keeps its
    * pre-/post-conditions reviewable in isolation. */

   return 0;
}
