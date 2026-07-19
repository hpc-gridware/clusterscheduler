/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2003 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <pthread.h>
#include <algorithm>
#include <cstring>
#include <vector>

#include "uti/sge_bootstrap_env.h"
#include "uti/sge_lock.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_ShareTree.h"
#include "sgeobj/ocs_UserProject.h"
#include "sgeobj/ocs_Usage.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/ocs_Session.h"

#include "evm/sge_event_master.h"

#include "sched/sgeee.h"

#include "comm/cl_commlib.h"

#include "uti/sge_profiling.h"
#include "uti/sge_thread_ctrl.h"

#include "gdi/ocs_gdi_security.h"
#include "gdi/ocs_gdi_Packet.h"

#include <cinttypes>
#include "ocs_SharetreeUsage.h"
#include "ocs_ReportingFileWriter.h"
#include "ocs_security_qmaster.h"
#include "setup_qmaster.h"
#include "sge_calendar_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_qmaster_timed_event.h"
#include "sge_host_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_give_jobs.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_thread_main.h"
#include "sge_thread_timer.h"
#include "sge_qmaster_heartbeat.h"
#include "sge_job_enforce_limit.h"
#include "sge_qmod_qmaster.h"
#include "reschedule.h"
#include "sge_job_qmaster.h"
#include "sge_log.h"

#include "msg_qmaster.h"

static void
sge_timer_cleanup_monitor(monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

/** CS-1239: drain the per-finish user/project dirty FIFOs to the spool backend.
 *
 * Self-rescheduling one-shot event: pulls up to a 100ms work budget from the
 * FIFOs under LOCK_GLOBAL (read), then re-arms itself at +1s while the queue
 * is still non-empty (resume-drain cadence), or at +STREE_SPOOL_INTERVAL when
 * caught up (idle cadence). The cadence knob lives in qmaster_params
 * (mconf_get_spool_time, default 240s).
 */
static void
sge_sharetree_spool_handler(te_event_t /* anEvent */, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_READ), monitor);
   const int remaining = ocs::SharetreeUsage::spool_budget(100, ocs::SessionManager::GDI_SESSION_NONE);
   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

   const uint32_t next_in_s = (remaining > 0) ? 1u : static_cast<uint32_t>(mconf_get_spool_time());
   te_event_t ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(next_in_s),
                                TYPE_SHARETREE_SPOOL_EVENT, ONE_TIME_EVENT,
                                0, 0, "sharetree-spool");
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

/** CS-1239 step 5: periodic share-tree tick - decay UU_/PR_/UPP_, then (if
 * anything is dirty) emit batched sgeE_USER_MOD / sgeE_PROJECT_MOD /
 * sgeE_NEW_SHARETREE inside one event-master transaction.
 *
 * Replaces the previous split into TYPE_USAGE_DECAY_EVENT (decay + per-user
 * MOD events) and TYPE_SHARETREE_PUBLISH_EVENT (republish). Pre-CS-1239 the
 * scheduler emitted USER_MOD / PROJECT_MOD / NEW_SHARETREE as one event
 * package on every share-tree-affecting order roundtrip; mirror clients
 * (including the scheduler thread itself) relied on that atomicity to
 * compute consistent tickets. The split path broke the invariant because
 * MOD events shipped on the worker thread at job-finish time and
 * NEW_SHARETREE shipped a tick later: between them the scheduler saw fresh
 * usage against a stale tree and dispatched the wrong jobs.
 *
 * This handler restores the invariant: one tick, one transaction, one event
 * package.
 *
 * Inside the tick (under LOCK_GLOBAL write):
 *   1. Decay every master user / project via decay_userprj_usage and mark
 *      each event-dirty (UniqueFifo dedup absorbs the case where worker
 *      booking already marked the same name in this interval). Master is
 *      the sole owner of usage decay - scheduler PASS 0 and sge_share_mon
 *      both read UU_/PR_usage at face value with no local catch-up, so
 *      the mass MOD storm IS the mechanism that keeps every mirror's
 *      UU_/PR_usage current.
 *   2. Refresh the default decay constant (formerly in sge_calc_tickets).
 *   3. If anything is dirty (event-dirty queues OR the share-tree-dirty
 *      flag), open a commit-required transaction:
 *        a. Emit sgeE_USER_MOD for each event-dirty user.
 *        b. Emit sgeE_PROJECT_MOD for each event-dirty project.
 *        c. Recompute master share tree (init nodes, count active ja_tasks
 *           per leaf, propagate counts, m_share, calc_node_usage, targets,
 *           STN_version bump).
 *        d. Emit sgeE_NEW_SHARETREE.
 *        e. Commit - the events ship as one package.
 *   4. Self-reschedule at +mconf_get_sharetree_tick_interval().
 *
 * Note on the leaf counting at step 3c: we walk the master job list and
 * count active ja_tasks per leaf via search_user_project_node, mirroring
 * what the scheduler does on its mirror copy but on master state. We
 * deliberately do NOT create temp "default" sibling nodes (a
 * scheduler-mirror-only construct), so default-mapped users see their
 * counts aggregated on the "default" leaf instead of split per-user.
 * Acceptable trade-off flagged in CS-1239 step 5.
 *
 * Seqno: handler keeps its own monotonic counter so we never collide with
 * the scheduler thread's sge_scheduling_run namespace and never get skipped
 * by the "seqno already processed" idempotency guard in
 * decay_userprj_usage. The calc_node_usage call inside the publish step
 * passes (u_long)-1 to recompute without stamping UU_/PR_usage_seqno
 * (that stamp is owned by the decay step a few lines above).
 *
 * Cadence: mconf_get_sharetree_tick_interval() returns the STREE_TICK_INTERVAL
 * qmaster_param value (default 5 s, clamped to [1, 300]). Runtime changes
 * take effect on the next tick; configuration_qmaster.cc also calls
 * sge_reschedule_sharetree_tick() to apply tighter intervals faster.
 */
static void
sge_sharetree_tick_handler(te_event_t /* anEvent */, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   static u_long tet_decay_run = 0;
   ++tet_decay_run;

   const uint64_t curr_time = sge_get_gmt64();
   lList *decay_list = ocs::Usage::get_decay_list();

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   /* CS-1239: short-circuit when no master share tree is configured. The
    * decay walk would mark every user / project event-dirty for nothing (no
    * consumer), and the publish step has nothing to publish. Self-reschedule
    * so a later qconf -Astree is picked up on the next tick. This restores
    * the pre-CS-1239 implicit gate ("no UU/PR objects -> no work"; see
    * calc_usage.md) by checking the direct indicator. Matching gate is in
    * sge_book_finished_job_usage. */
   if (lFirst(*ocs::DataStore::get_master_list(SGE_TYPE_SHARETREE)) == nullptr) {
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

      const uint32_t next_in_s = static_cast<uint32_t>(mconf_get_sharetree_tick_interval());
      te_event_t ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(next_in_s),
                                   TYPE_SHARETREE_TICK_EVENT, ONE_TIME_EVENT,
                                   0, 0, "sharetree-tick");
      te_add_event(ev);
      te_free_event(&ev);

      lFreeList(&decay_list);
      DRETURN_VOID;
   }

   /* 1+2: decay all users / projects on the master GLOBAL store and mark each
    *      one event-dirty so the publish step below ships sgeE_USER_MOD /
    *      sgeE_PROJECT_MOD to every mirror. With CS-1239 the master is the
    *      sole owner of usage decay: scheduler PASS 0 and sge_share_mon both
    *      read UU_/PR_usage at face value (no local catch-up), so the mirror
    *      must carry the current decayed value or those consumers compute
    *      against a stale historic value. The mass mark is the price of "one
    *      authoritative decay site"; spool-dirty stays worker-only because
    *      the spool snapshot doesn't need to reflect every decay tick (the
    *      load-time stamp lets the recovery-side catch-up resolve it).
    *      Refresh the default decay constant in case halftime changed. */
   ocs::Usage::calculate_default_decay_constant(sconf_get_halftime());

   bool any_object = false;
   lList **user_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_USER);
   if (user_list != nullptr) {
      for_each_rw_lv(user, *user_list) {
         ocs::UserProject::decay_userprj_usage(user, true, decay_list, tet_decay_run, curr_time);
         ocs::SharetreeUsage::mark_user_event_dirty(lGetString(user, UU_name));
         any_object = true;
      }
   }
   lList **project_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_PROJECT);
   if (project_list != nullptr) {
      for_each_rw_lv(project, *project_list) {
         ocs::UserProject::decay_userprj_usage(project, false, decay_list, tet_decay_run, curr_time);
         ocs::SharetreeUsage::mark_project_event_dirty(lGetString(project, PR_name));
         any_object = true;
      }
   }
   if (any_object) {
      ocs::SharetreeUsage::mark_share_tree_dirty();
   }

   /* 3: if anything is dirty, open a transaction and ship the batched
    *    USER_MOD / PROJECT_MOD / NEW_SHARETREE as one event package. */
   const bool tree_dirty = ocs::SharetreeUsage::consume_share_tree_dirty();
   const bool any_events = ocs::SharetreeUsage::has_event_dirty();

   if (tree_dirty || any_events) {
      lList **master_stree_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_SHARETREE);
      lListElem *root = (master_stree_list != nullptr) ? lFirstRW(*master_stree_list) : nullptr;

      sge_set_commit_required();

      ocs::SharetreeUsage::emit_dirty_user_events(ocs::SessionManager::GDI_SESSION_NONE);
      ocs::SharetreeUsage::emit_dirty_project_events(ocs::SessionManager::GDI_SESSION_NONE);

      if (root != nullptr) {
         const lList *master_user_list = *ocs::DataStore::get_master_list(SGE_TYPE_USER);
         const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
         const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

         sge_init_share_tree_nodes(root);
         ocs::ShareTree::set_node_project_flag(root, master_project_list);

         /* Active-ja_task predicate matches scheduler's job_is_active. */
         for_each_ep_lv(job, master_job_list) {
            for_each_rw_lv(ja_task, lGetList(job, JB_ja_tasks)) {
               const u_long status = lGetUlong(ja_task, JAT_status);
               const u_long state = lGetUlong(ja_task, JAT_state);
               const bool active = (status & (JRUNNING | JMIGRATING | JTRANSFERING))
                                && !(state & (JSUSPENDED | JSUSPENDED_ON_THRESHOLD));
               if (!active) {
                  continue;
               }
               const char *owner = lGetString(job, JB_owner);
               const char *project = lGetString(job, JB_project);
               if (owner == nullptr && project == nullptr) {
                  continue;
               }
               lListElem *parent = nullptr;
               lListElem *leaf = ocs::ShareTree::search_user_project_node(root, owner, project,
                                                                         &parent, root);
               if (leaf != nullptr) {
                  lSetUlong(leaf, STN_job_ref_count,
                            lGetUlong(leaf, STN_job_ref_count) + 1);
                  lSetUlong(leaf, STN_active_job_ref_count,
                            lGetUlong(leaf, STN_active_job_ref_count) + 1);
               }
            }
         }

         update_job_ref_count(root);
         update_active_job_ref_count(root);
         lSetDouble(root, STN_m_share, 1.0);
         calculate_m_shares(root);

         /* Seqno (u_long)-1 tells calc_node_usage to recurse + recompute
          * without stamping UU_/PR_usage_seqno - the decay step above owns
          * that stamp and we must not double-stamp inside the same tick. */
         ocs::ShareTree::calc_node_usage(root, master_user_list, master_project_list,
                                         decay_list, curr_time, nullptr, (u_long)-1);
         sge_calc_node_targets(root, root);

         lAddUlong(root, STN_version, 1);

         sge_add_event(curr_time, sgeE_NEW_SHARETREE, 0, 0, nullptr, nullptr,
                       nullptr, root, ocs::SessionManager::GDI_SESSION_NONE);
      }

      sge_commit(ocs::SessionManager::GDI_SESSION_NONE);
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   const uint32_t next_in_s = static_cast<uint32_t>(mconf_get_sharetree_tick_interval());
   te_event_t ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(next_in_s),
                                TYPE_SHARETREE_TICK_EVENT, ONE_TIME_EVENT,
                                0, 0, "sharetree-tick");
   te_add_event(ev);
   te_free_event(&ev);

   lFreeList(&decay_list);
   DRETURN_VOID;
}

/**
 * @brief CS-1908 retention sweep handler.
 *
 * Periodically walks master_job_list, identifies finished ja_tasks eligible for
 * pruning by time (JAT_end_time + finished_jobs_keep_time < now) or by count
 * (JAT falls outside the newest finished_jobs_max globally) — whichever bites
 * first, per-tick-bounded by finished_jobs_sweep_batch (R18). Eligible JATs
 * are pruned via sge_bury_ja_task, which owns its own BDB transaction per
 * KTD6 (no outer wrap; the sweep loops one JAT at a time).
 *
 * Runs under LOCK_GLOBAL write, matching the sge_sharetree_tick_handler
 * pattern.
 *
 * The event carries a "sweep_all" flag in aKey1: when 0 (normal), the tick
 * respects retention gates and short-circuits if both are 0 (feature off);
 * when 1, the tick drains every JFINISHED ja_task on master_job_list
 * regardless of retention state (still capped by sweep_batch per tick). The
 * sweep_all=1 mode is activated by sge_reschedule_finished_jobs_sweep() when
 * retention has just been turned off via qconf -mconf, and by the bootstrap
 * tick after qmaster start (in case retained ja_tasks were reloaded from
 * spool for a cluster whose retention was disabled between shutdowns).
 * The handler keeps rescheduling with sweep_all=1 until the drain is complete
 * (finished.size() <= sweep_batch), then transitions to the normal cadence.
 */
static void
sge_finished_jobs_sweep_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const uint32_t keep_time = mconf_get_finished_jobs_keep_time();
   const uint32_t max_finished = mconf_get_finished_jobs_max();
   const uint32_t sweep_interval = static_cast<uint32_t>(mconf_get_finished_jobs_sweep_interval());
   const bool sweep_all = (te_get_first_numeric_key(anEvent) != 0);

   /* Feature off AND no drain requested: skip the walk entirely, still
    * self-reschedule at the normal cadence so a runtime qconf -mconf enable
    * is picked up on the next tick. */
   if (!sweep_all && keep_time == 0 && max_finished == 0) {
      te_event_t ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(sweep_interval),
                                   TYPE_FINISHED_JOBS_SWEEP_EVENT, ONE_TIME_EVENT,
                                   0, 0, "finished-jobs-sweep");
      te_add_event(ev);
      te_free_event(&ev);
      DRETURN_VOID;
   }

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   struct PruneCandidate {
      lListElem *job;
      lListElem *ja_task;
      uint64_t end_time;
   };

   /* Phase 1: enumerate all finished ja_tasks with their end_time. Store raw
    * pointers into master_job_list -- we hold LOCK_GLOBAL write for the whole
    * tick, so nothing else can mutate the list, the JBs, or their JAT
    * sublists. Phase 3 uses the stored pointers directly, avoiding a per-JAT
    * ID relookup. sge_bury_ja_task on entry i removes only that JAT (and
    * possibly its whole JB if entry i was the last JAT on it), which cannot
    * invalidate any other entry's pointers: each JAT appears at most once,
    * and if a JB is whole-removed at entry i then no other entry references
    * it (that would contradict the "last remaining JAT" precondition). */
   std::vector<PruneCandidate> finished;
   lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   for_each_rw_lv(job, master_job_list) {
      for_each_rw_lv(ja_task, lGetListRW(job, JB_ja_tasks)) {
         if (lGetUlong(ja_task, JAT_status) == JFINISHED) {
            finished.push_back({job, ja_task, lGetUlong64(ja_task, JAT_end_time)});
         }
      }
   }

   /* Phase 2: compute eligibility. Sort by end_time ascending so oldest come
    * first — lets us mark the count-eligible prefix trivially and gives us
    * predictable time-based eviction order. */
   std::sort(finished.begin(), finished.end(),
             [](const PruneCandidate &a, const PruneCandidate &b) {
                return a.end_time < b.end_time;
             });

   const size_t count_overflow = (max_finished > 0 && finished.size() > max_finished)
                                    ? finished.size() - max_finished
                                    : 0;
   const uint64_t curr_time = sge_get_gmt64();
   const uint64_t keep_time_delta = (keep_time > 0) ? sge_gmt32_to_gmt64(keep_time) : 0;
   const uint32_t sweep_batch = static_cast<uint32_t>(mconf_get_finished_jobs_sweep_batch());

   std::vector<PruneCandidate> eligible;
   for (size_t i = 0; i < finished.size() && eligible.size() < sweep_batch; ++i) {
      if (sweep_all) {
         /* Drain mode: every finished ja_task is eligible until sweep_batch
          * bites. Ignores keep_time / max — used when retention has just
          * been turned off (via qconf -mconf) or on qmaster start-up with
          * retained ja_tasks reloaded from spool. */
         eligible.push_back(finished[i]);
         continue;
      }
      const bool count_eligible = (i < count_overflow);
      const bool time_eligible = (keep_time > 0 && finished[i].end_time > 0 &&
                                  finished[i].end_time + keep_time_delta < curr_time);
      if (count_eligible || time_eligible) {
         eligible.push_back(finished[i]);
      }
   }

   /* Phase 3: prune. Pointers stored in phase 1 remain valid under the
    * still-held LOCK_GLOBAL write (see PruneCandidate comment above). */
   const char *sge_root = bootstrap_get_sge_root();
   for (const PruneCandidate &c : eligible) {
      sge_bury_ja_task(sge_root, c.job, lGetUlong(c.job, JB_job_number),
                       c.ja_task, 1 /* spool_job */, 0 /* no_events */,
                       ocs::SessionManager::GDI_SESSION_NONE);
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   /* Self-reschedule at +sweep_interval. Runtime tunable changes take effect
    * on the next tick; sge_reschedule_finished_jobs_sweep() (called from
    * configuration_qmaster.cc after qconf -mconf) drops the pending event and
    * re-queues at +5 s for a tighter response to interval shortening.
    *
    * Carry sweep_all=1 forward when we were in drain mode AND there are still
    * finished ja_tasks beyond this tick's sweep_batch to clear -- the next
    * tick continues the drain. Otherwise the drain is complete (or this was
    * a normal tick) and the next tick uses sweep_all=0. */
   const uint32_t next_sweep_all = (sweep_all && finished.size() > sweep_batch) ? 1u : 0u;
   te_event_t ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(sweep_interval),
                                TYPE_FINISHED_JOBS_SWEEP_EVENT, ONE_TIME_EVENT,
                                next_sweep_all, 0, "finished-jobs-sweep");
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

/* sge_reschedule_finished_jobs_sweep() lives in ocs_FinishedJobs.cc so it can
 * be linked into consumers (e.g. lightweight test binaries) that do not pull
 * in this whole translation unit. See ocs_FinishedJobs.h. */

/****** qmaster/sge_thread_timer/sge_timer_register_event_handler() *************
*  NAME
*     sge_timer_register_event_handler() -- register event handlers
*
*  SYNOPSIS
*     void sge_timer_register_event_handler()
*
*  FUNCTION
*    registers event handlers
*
*  SEE ALSO
*     sge_thread_timer/sge_timer_start_periodic_tasks
*******************************************************************************/
void
sge_timer_register_event_handler() {
   DENTER(TOP_LAYER);

   /*
    * recurring events
    */

   te_register_event_handler(sge_store_job_number, TYPE_JOB_NUMBER_EVENT);

   te_register_event_handler(sge_store_ar_id, TYPE_AR_ID_EVENT);

   te_register_event_handler(sge_load_value_cleanup_handler, TYPE_LOAD_VALUE_CLEANUP_EVENT);

   te_register_event_handler(sge_automatic_user_cleanup_handler, TYPE_AUTOMATIC_USER_CLEANUP_EVENT);

   te_register_event_handler(ocs::SessionManager::session_cleanup_handler, TYPE_SESSION_CLEANUP_EVENT);

   te_register_event_handler(sge_sharetree_spool_handler, TYPE_SHARETREE_SPOOL_EVENT);

   te_register_event_handler(sge_sharetree_tick_handler, TYPE_SHARETREE_TICK_EVENT);

   te_register_event_handler(sge_finished_jobs_sweep_handler, TYPE_FINISHED_JOBS_SWEEP_EVENT);

   /*
    * one time events
    */

   te_register_event_handler(sge_job_resend_event_handler, TYPE_JOB_RESEND_EVENT);

   te_register_event_handler(sge_job_enfoce_limit_handler, TYPE_ENFORCE_LIMIT_EVENT);

   te_register_event_handler(sge_calendar_event_handler, TYPE_CALENDAR_EVENT);

   te_register_event_handler(resend_signal_event, TYPE_SIGNAL_RESEND_EVENT);

   te_register_event_handler(reschedule_unknown_event, TYPE_RESCHEDULE_UNKNOWN_EVENT);

   te_register_event_handler(sge_ar_event_handler, TYPE_AR_EVENT);

   DRETURN_VOID;
}

/****** qmaster/sge_thread_timer/sge_timer_start_periodic_tasks() ************************
*  NAME
*     sge_timer_start_periodic_tasks() -- Start periodic qmaster tasks.
*
*  SYNOPSIS
*     static void sge_timer_start_periodic_tasks()
*
*  FUNCTION
*     Start periodic qmaster tasks. Periodic tasks are implemented as recurring
*     events.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none
*
*  NOTES
*******************************************************************************/
void sge_timer_start_periodic_tasks() {
   te_event_t ev = nullptr;

   DENTER(TOP_LAYER);

   /* recurring events */
   ev = te_new_event(sge_gmt32_to_gmt64(15), TYPE_JOB_NUMBER_EVENT, RECURRING_EVENT, 0, 0, "job_number_changed");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(15), TYPE_AR_ID_EVENT, RECURRING_EVENT, 0, 0, "ar_id_changed");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(15), TYPE_LOAD_VALUE_CLEANUP_EVENT, RECURRING_EVENT, 0, 0, "load-value-cleanup");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(60), TYPE_AUTOMATIC_USER_CLEANUP_EVENT, RECURRING_EVENT, 0, 0, "automatic-user-cleanup");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(120), TYPE_SESSION_CLEANUP_EVENT, RECURRING_EVENT, 0, 0, "session-cleanup");
   te_add_event(ev);
   te_free_event(&ev);

   /* CS-1239: bootstrap the share-tree spool event at +5 s so a freshly
    * started qmaster drains the per-finish dirty FIFOs within seconds
    * rather than waiting up to STREE_SPOOL_INTERVAL (240 s default). The
    * handler self-reschedules (ONE_TIME_EVENT, not RECURRING_EVENT) so the
    * next due time can depend on how much work is left after each tick.
    * Same caveat as for the decay event: a runtime change to
    * STREE_SPOOL_INTERVAL takes effect only on the *next* tick, so
    * configuration_qmaster.cc::do_mod_config also calls
    * sge_reschedule_sharetree_spool() to drop the pending event and
    * re-queue at +5 s. */
   ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(5),
                     TYPE_SHARETREE_SPOOL_EVENT, ONE_TIME_EVENT, 0, 0, "sharetree-spool");
   te_add_event(ev);
   te_free_event(&ev);

   /* CS-1239: bootstrap the periodic share-tree tick (decay + batched
    * USER_MOD/PROJECT_MOD/NEW_SHARETREE publish) at +5 s so a freshly
    * started qmaster has STN_combined_usage / m_share populated for
    * sge_share_mon within seconds. After the first tick the handler
    * self-reschedules at +STREE_TICK_INTERVAL. ONE_TIME_EVENT +
    * handler-side reschedule means a runtime change to STREE_TICK_INTERVAL
    * takes effect on the next tick. Config changes that need to take effect
    * *sooner* go through sge_reschedule_sharetree_tick() from
    * configuration_qmaster.cc. */
   ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(5),
                     TYPE_SHARETREE_TICK_EVENT, ONE_TIME_EVENT, 0, 0, "sharetree-tick");
   te_add_event(ev);
   te_free_event(&ev);

   /* CS-1908: bootstrap the finished-job retention sweep at +5 s. Passed with
    * sweep_all=1 (aKey1=1) so a freshly-started qmaster whose retention was
    * disabled between shutdowns still drains any retained ja_tasks that
    * reloaded from spool. If nothing is retained the first tick short-
    * circuits after a cheap master_job_list walk and reschedules
    * sweep_all=0; if retention is enabled the first tick honours the
    * tunables and starts pruning. The handler self-reschedules at
    * +finished_jobs_sweep_interval. Config changes that need to take effect
    * sooner go through sge_reschedule_finished_jobs_sweep() from
    * configuration_qmaster.cc. */
   ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(5),
                     TYPE_FINISHED_JOBS_SWEEP_EVENT, ONE_TIME_EVENT, 1, 0, "finished-jobs-sweep");
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

void
sge_timer_initialize(monitoring_t *monitor) {
   cl_thread_settings_t *dummy_thread_p = nullptr;
   lList *answer_list = nullptr;

   DENTER(TOP_LAYER);

   te_init();
   DPRINTF("timed event module has been initialized\n");
   heartbeat_initialize();
   DPRINTF("heartbeat module initialized\n");
   ar_initialize_timer(&answer_list, monitor, ocs::SessionManager::GDI_SESSION_NONE);
   answer_list_output(&answer_list);
   DPRINTF("ar and corresponding timers are initialized\n");
   calendar_initalize_timer(monitor);
   DPRINTF("queue states and corresponding timers are initialized due to calendar settings\n");
   host_initalitze_timer();
   DPRINTF("reschedule unknown timer have been initialized\n");
   sge_timer_register_event_handler();
   DPRINTF("timer are registered at timed event module\n");
   sge_timer_start_periodic_tasks();
   DPRINTF("periodic tasks are registered at timed event module\n");
   sge_initialize_persistance_timer();
   DPRINTF("persistence timer initialized at timed event module\n");
   sge_setup_job_resend();
   DPRINTF("job resend functionality initialized\n");
   sge_add_check_limit_trigger();
   DPRINTF("added timer event to check load reports and possibly to enforce limits\n");
#if defined(OCS_WITH_OPENSSL)
   ocs::qmaster::cert_renewal_initialize();
   DPRINTF("added timer event to check certificate renewal\n");
#endif

   DPRINTF(SFN " related initialisation has been done\n", to_cstr(TIMER_THREAD));

   cl_thread_list_setup(&(Main_Control.timer_thread_pool), "timer thread pool");
   cl_thread_list_create_thread(Main_Control.timer_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                to_cstr(TIMER_THREAD), 0, sge_timer_main, nullptr, nullptr, CL_TT_TIMER);
   DRETURN_VOID;
}

void
sge_timer_terminate() {
   DENTER(TOP_LAYER);

   cl_thread_list_elem_t *thread_elem;
   cl_thread_list_elem_t *next_thread_elem = cl_thread_list_get_first_elem(Main_Control.timer_thread_pool);
   while ((thread_elem = next_thread_elem) != nullptr) {
      next_thread_elem = cl_thread_list_get_next_elem(thread_elem);

      cl_thread_shutdown(thread_elem->thread_config);
   }

   // wake up the timer thread(s)
   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);
   pthread_cond_broadcast(&Event_Control.cond_var);
   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.timer_thread_pool);
   while (thread != nullptr) {
      DPRINTF("getting canceled\n");
      cl_thread_list_delete_thread(Main_Control.timer_thread_pool, thread);

      thread = cl_thread_list_get_first_thread(Main_Control.timer_thread_pool);
   }
   DPRINTF("all " SFN " threads terminated\n", to_cstr(TIMER_THREAD));
   te_shutdown();

   DPRINTF(SFN " related cleanup has been done\n", to_cstr(TIMER_THREAD));
   INFO(MSG_THREAD_XTERMINATED_S, to_cstr(TIMER_THREAD));

   DRETURN_VOID;
}

/****** qmaster/sge_qmaster_timed_event/timed_event_thread() ***********************
*  NAME
*     timed_event_thread() -- Deliver timed events due
*
*  SYNOPSIS
*     static void* timed_event_thread(void* anArg)
*
*  FUNCTION
*     Check whether system clock has been put back. If so, adjust event due
*     times. Check if event list does contain events. If so, fetch first event
*     and check whether it is due. If there is a due event, search event handler
*     table for a matching event handler and invoke it.
*
*     After event delivery an event with event mode 'ONE_TIME_EVENT' will be
*     removed. An event with event mode 'RECURRING_EVENT' will be delivered
*     repeatedly.
*
*     The event list MUST be sorted in ascending event due time order.
*
*  INPUTS
*     void* anArg - not used
*
*  RESULT
*     void* - none
*
*  NOTES
*     MT-NOTE: 'timed_event_thread()' is a thread function. Do NOT use this
*     MT-NOTE: function in any other way!
*     MT-NOTE:
*     MT-NOTE: If the event list is empty, 'timed_event_thread()' will wait until
*     MT-NOTE: an event has been added.
*     MT-NOTE:
*     MT-NOTE: If no event is due, i.e. the due date of the next event does lie
*     MT-NOTE: ahead, 'timed_event_thread()' does wait until the next event does
*     MT-NOTE: become due, or an event which is due earlier has been added. If
*     MT-NOTE: an event has been deleted while waiting ('Event_Control.deleted'
*     MT-NOTE: equals 'true'), skip the current event and start over. The
*     MT-NOTE: deleted event maybe the event 'timed_event_thread()' has been
*     MT-NOTE: waiting for.
*     MT-NOTE:
*     MT-NOTE: Before 'te_scan_table_and_deliver()' is invoked,
*     MT-NOTE: 'Event_Control.mutex' MUST be unlocked. Otherwise, a deadlock
*     MT-NOTE: may occur due to recursive mutex locking.
*
*******************************************************************************/
[[noreturn]] void *
sge_timer_main(void *arg) {
   DENTER(TOP_LAYER);

   auto *thread_config = (cl_thread_settings_t *) arg;
   monitoring_t monitor;
   monitoring_t *p_monitor = &monitor;

   lListElem *le = nullptr;
   te_event_t te = nullptr;
   uint64_t now;
   uint64_t next_prof_output = 0;

   DPRINTF("started\n");

   // set the thread name and id used by logging and others
   const char *thread_name = thread_config->thread_name;
   int thread_id = thread_config->thread_id;
   component_set_thread_name(thread_name);
   component_set_thread_id(thread_id);
   DPRINTF(SFN "(%d) started\n", thread_name, thread_id);

   // init monitoring
   cl_thread_func_startup(thread_config);
   sge_monitor_init(p_monitor, thread_config->thread_name, TET_EXT, TET_WARNING, TET_ERROR,
                    ocs::ReportingFileWriter::create_monitoring_records);
   sge_qmaster_thread_init(QMASTER, TIMER_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "TEvent Thread");
   conf_update_thread_profiling("TEvent Thread");

   while (true) {
      int execute = 0;
      thread_start_stop_profiling();

      sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

      te_check_time(sge_get_gmt64());

      Event_Control.last = sge_get_gmt64();

      MONITOR_IDLE_TIME(te_wait_empty(), p_monitor, mconf_get_monitoring_options());
      MONITOR_MESSAGES(p_monitor);

      MONITOR_TET_COUNT(p_monitor);
      MONITOR_TET_EVENT(p_monitor, lGetNumberOfElem(Event_Control.list));

      le = lFirstRW(Event_Control.list);
      te = te_event_from_list_elem(le);
      now = Event_Control.next = sge_get_gmt64();

      if (te->when > now) {
         Event_Control.next = te->when;
         Event_Control.deleted = false;
         MONITOR_IDLE_TIME(te_wait_next(te, now), p_monitor, mconf_get_monitoring_options());

         if ((Event_Control.next < te->when) || Event_Control.deleted) {
            DPRINTF("%s: event list changed - next:" sge_u64 " --> start over\n", __func__, Event_Control.next);

            sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

            if (!sge_thread_has_shutdown_started()) {
               te_free_event(&te);
               sge_monitor_output(p_monitor);
               continue;
            }
         }
      }

      MONITOR_TET_EXEC(p_monitor);

      lDechainElem(Event_Control.list, le);
      lFreeElem(&le);

      sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

      if (!sge_thread_has_shutdown_started()) {
         te_scan_table_and_deliver(te, p_monitor);
      }
      te_free_event(&te);

      sge_monitor_output(p_monitor);
      thread_output_profiling("timed event thread profiling summary:", &next_prof_output);

      /* pthread cancellation point */
      do {
         pthread_cleanup_push((void (*)(void *)) sge_timer_cleanup_monitor, (void *) p_monitor);
         cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute);

         sge_thread_usleep_during_shutdown();
      } while (sge_thread_has_shutdown_started());
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()
}

