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

#include <string>

#include "evm/sge_event_master.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userprj.h"

#include "uti/ocs_UniqueFifo.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "ocs_SharetreeUsage.h"
#include "sge_persistence_qmaster.h"
#include "sge_qmaster_timed_event.h"

namespace {
   /* Spool FIFOs - drained by the TET share-tree spool handler, no event
    * traffic, just disk writes. */
   ocs::UniqueFifo<std::string> dirty_users_spool;
   ocs::UniqueFifo<std::string> dirty_projects_spool;

   /* Event FIFOs - drained by the TET share-tree tick handler inside one
    * event-master transaction together with sgeE_NEW_SHARETREE. */
   ocs::UniqueFifo<std::string> dirty_users_event;
   ocs::UniqueFifo<std::string> dirty_projects_event;

   /* CS-1239 step 5: single bool, no FIFO - mirror clients only care that
    * "something changed", not which leaves. Guarded by LOCK_GLOBAL like the
    * FIFOs above. Starts true so the +5s bootstrap publish in
    * sge_timer_start_periodic_tasks always populates a freshly started
    * qmaster's master share tree with combined_usage / m_share. */
   bool share_tree_dirty = true;

   /* Spool one user by name. Skips silently if the user has been removed
    * between mark_user_spool_dirty and the flush (qconf -duser race). */
   void spool_one_user(const std::string &name, uint64_t gdi_session) {
      const lList *master_user_list = *ocs::DataStore::get_master_list(SGE_TYPE_USER);
      lListElem *u = user_list_locate(master_user_list, name.c_str());
      if (u == nullptr) {
         return;
      }
      lList *answer_list = nullptr;
      sge_event_spool(&answer_list, 0, sgeE_USER_MOD, 0, 0, name.c_str(),
                      nullptr, nullptr, u, nullptr, nullptr, false, true, gdi_session);
      answer_list_output(&answer_list);
   }

   /* Spool one project by name. Skips silently if removed in the meantime. */
   void spool_one_project(const std::string &name, uint64_t gdi_session) {
      const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
      lListElem *p = prj_list_locate(master_project_list, name.c_str());
      if (p == nullptr) {
         return;
      }
      lList *answer_list = nullptr;
      sge_event_spool(&answer_list, 0, sgeE_PROJECT_MOD, 0, 0, name.c_str(),
                      nullptr, nullptr, p, nullptr, nullptr, false, true, gdi_session);
      answer_list_output(&answer_list);
   }

   /* Emit sgeE_USER_MOD for one user. Caller owns the event-master
    * transaction. Skips silently if the user was removed since the mark. */
   void emit_one_user_event(const std::string &name, uint64_t gdi_session) {
      const lList *master_user_list = *ocs::DataStore::get_master_list(SGE_TYPE_USER);
      lListElem *u = user_list_locate(master_user_list, name.c_str());
      if (u == nullptr) {
         return;
      }
      sge_add_event(0, sgeE_USER_MOD, 0, 0, name.c_str(), nullptr, nullptr, u, gdi_session);
   }

   /* Emit sgeE_PROJECT_MOD for one project. Same contract as the user
    * helper above. */
   void emit_one_project_event(const std::string &name, uint64_t gdi_session) {
      const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
      lListElem *p = prj_list_locate(master_project_list, name.c_str());
      if (p == nullptr) {
         return;
      }
      sge_add_event(0, sgeE_PROJECT_MOD, 0, 0, name.c_str(), nullptr, nullptr, p, gdi_session);
   }
}

/** Mark a user object dirty for spooling. Names are de-duplicated: a
 *  second call with the same name while the first is still queued is
 *  a no-op.
 *
 *  @param name the user
 */
void
ocs::SharetreeUsage::mark_user_spool_dirty(const char *name) {
   if (name == nullptr) {
      return;
   }
   dirty_users_spool.push(name);
}

/** Mark a project object dirty for spooling. De-duplicated.
 *  @param name the project
 */
void
ocs::SharetreeUsage::mark_project_spool_dirty(const char *name) {
   if (name == nullptr) {
      return;
   }
   dirty_projects_spool.push(name);
}

/** Mark a user object dirty for event emission. De-duplicated. The
 *  next share-tree tick will emit sgeE_USER_MOD for this user inside
 *  the publish transaction.
 *
 *  @param name the user
 */
void
ocs::SharetreeUsage::mark_user_event_dirty(const char *name) {
   if (name == nullptr) {
      return;
   }
   dirty_users_event.push(name);
}

/** Mark a project object dirty for event emission. De-duplicated.
 *  @param name the project
 */
void
ocs::SharetreeUsage::mark_project_event_dirty(const char *name) {
   if (name == nullptr) {
      return;
   }
   dirty_projects_event.push(name);
}

/** Returns true if either event FIFO has at least one entry. The
 *  share-tree tick handler uses this to decide whether to open an
 *  event-master transaction at all.
 *
 *  @return true when either FIFO has an entry
 */
bool
ocs::SharetreeUsage::has_event_dirty() {
   return !dirty_users_event.empty() || !dirty_projects_event.empty();
}

/** Drain the user event FIFO, emitting sgeE_USER_MOD for each entry
 *  via sge_add_event. Must be called inside an open event-master
 *  transaction (sge_set_commit_required ... sge_commit) so the events
 *  ship as one package with the trailing sgeE_NEW_SHARETREE.
 *
 *  @param gdi_session the session the events belong to
 */
void
ocs::SharetreeUsage::emit_dirty_user_events(uint64_t gdi_session) {
   while (!dirty_users_event.empty()) {
      emit_one_user_event(dirty_users_event.pop_front(), gdi_session);
   }
}

/** Drain the project event FIFO, emitting sgeE_PROJECT_MOD for each
 *  entry. Same transaction-context contract as emit_dirty_user_events.
 *
 *  @param gdi_session the session the events belong to
 */
void
ocs::SharetreeUsage::emit_dirty_project_events(uint64_t gdi_session) {
   while (!dirty_projects_event.empty()) {
      emit_one_project_event(dirty_projects_event.pop_front(), gdi_session);
   }
}

/** Drain both event FIFOs entirely, emitting the queued MOD events.
 *  Called from the qmaster shutdown sequence so finish-driven events
 *  do not get dropped on the last tick interval. Caller is responsible
 *  for the surrounding transaction (or for accepting the per-event
 *  package overhead if shutdown speed beats atomicity here).
 *
 *  @param gdi_session the session the events belong to
 */
void
ocs::SharetreeUsage::emit_events_all(uint64_t gdi_session) {
   emit_dirty_user_events(gdi_session);
   emit_dirty_project_events(gdi_session);
}

/** Drain the head of the user and project spool FIFOs to the spool
 *  backend for up to budget_ms milliseconds. Returns the residual queue
 *  size (sum of users + projects still spool-dirty). A non-zero return
 *  means the caller should reschedule sooner; zero means the next idle
 *  gap is fine.
 *
 *  @param budget_ms how long to spend spooling
 *  @param gdi_session the session the change belongs to
 *  @return the residual queue size
 */
int
ocs::SharetreeUsage::spool_budget(int budget_ms, uint64_t gdi_session) {
   const uint64_t budget_end = sge_get_gmt64() + static_cast<uint64_t>(budget_ms) * 1000;
   // @todo We might want to get master_user_list and master_project_list here once and pass it
   //       to spool_one_user() and spool_one_project().

   /* Interleave users and projects so neither side starves the other when a
    * burst hits one of them. */
   while (sge_get_gmt64() < budget_end) {
      bool did_work = false;
      if (!dirty_users_spool.empty()) {
         spool_one_user(dirty_users_spool.pop_front(), gdi_session);
         did_work = true;
      }
      if (!dirty_projects_spool.empty()) {
         spool_one_project(dirty_projects_spool.pop_front(), gdi_session);
         did_work = true;
      }
      if (!did_work) {
         break;
      }
   }

   return static_cast<int>(dirty_users_spool.size() + dirty_projects_spool.size());
}

/** Drain both spool FIFOs entirely, ignoring any time budget. Called
 *  from the qmaster shutdown sequence before the spool backend is torn
 *  down.
 *
 *  @param gdi_session the session the change belongs to
 */
void
ocs::SharetreeUsage::spool_all(uint64_t gdi_session) {
   while (!dirty_users_spool.empty()) {
      spool_one_user(dirty_users_spool.pop_front(), gdi_session);
   }
   while (!dirty_projects_spool.empty()) {
      spool_one_project(dirty_projects_spool.pop_front(), gdi_session);
   }
}

/** CS-1239 step 5: mark the master share tree as "needs republish".
 *  Set by the worker thread after sge_book_finished_job_usage sums a
 *  finish into UU_/PR_/UPP_ usage, and by the TET decay handler after
 *  a decay pass. Consumed (and cleared) by the TET share-tree tick
 *  handler when it next emits sgeE_NEW_SHARETREE.
 */
void
ocs::SharetreeUsage::mark_share_tree_dirty() {
   share_tree_dirty = true;
}

/** Returns true and clears the flag atomically (under LOCK_GLOBAL).
 *  The tick handler uses this so the "saw dirty -> clear -> publish"
 *  sequence cannot lose a concurrent mark.
 *
 *  @return true when the share tree needed republishing
 */
bool
ocs::SharetreeUsage::consume_share_tree_dirty() {
   const bool was_dirty = share_tree_dirty;
   share_tree_dirty = false;
   return was_dirty;
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
sge_reschedule_sharetree_tick() {
   DENTER(TOP_LAYER);

   te_delete_one_time_event(TYPE_SHARETREE_TICK_EVENT, 0, 0, "sharetree-tick");

   te_event_t ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(5),
                                TYPE_SHARETREE_TICK_EVENT, ONE_TIME_EVENT,
                                0, 0, "sharetree-tick");
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

/** CS-1239: re-schedule the periodic share-tree spool event. Same
 *  rationale as sge_reschedule_sharetree_tick() but for STREE_SPOOL_INTERVAL:
 *  a config change to STREE_SPOOL_INTERVAL would otherwise only take effect
 *  after the already-queued event fires (up to 240 s away on default
 *  configs), which made tests like issue_1385 - which pin the interval and
 *  read the spool file shortly after - flake. Drops the pending event and
 *  re-queues at +5 s. Same thread-safety / no-op-if-not-queued guarantees. */
void
sge_reschedule_sharetree_spool() {
   DENTER(TOP_LAYER);

   te_delete_one_time_event(TYPE_SHARETREE_SPOOL_EVENT, 0, 0, "sharetree-spool");

   te_event_t ev = te_new_event(sge_get_gmt64() + sge_gmt32_to_gmt64(5),
                                TYPE_SHARETREE_SPOOL_EVENT, ONE_TIME_EVENT,
                                0, 0, "sharetree-spool");
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}
