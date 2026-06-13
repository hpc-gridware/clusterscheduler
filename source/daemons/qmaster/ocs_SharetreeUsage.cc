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

void
ocs::SharetreeUsage::mark_user_spool_dirty(const char *name) {
   if (name == nullptr) {
      return;
   }
   dirty_users_spool.push(name);
}

void
ocs::SharetreeUsage::mark_project_spool_dirty(const char *name) {
   if (name == nullptr) {
      return;
   }
   dirty_projects_spool.push(name);
}

void
ocs::SharetreeUsage::mark_user_event_dirty(const char *name) {
   if (name == nullptr) {
      return;
   }
   dirty_users_event.push(name);
}

void
ocs::SharetreeUsage::mark_project_event_dirty(const char *name) {
   if (name == nullptr) {
      return;
   }
   dirty_projects_event.push(name);
}

bool
ocs::SharetreeUsage::has_event_dirty() {
   return !dirty_users_event.empty() || !dirty_projects_event.empty();
}

void
ocs::SharetreeUsage::emit_dirty_user_events(uint64_t gdi_session) {
   while (!dirty_users_event.empty()) {
      emit_one_user_event(dirty_users_event.pop_front(), gdi_session);
   }
}

void
ocs::SharetreeUsage::emit_dirty_project_events(uint64_t gdi_session) {
   while (!dirty_projects_event.empty()) {
      emit_one_project_event(dirty_projects_event.pop_front(), gdi_session);
   }
}

void
ocs::SharetreeUsage::emit_events_all(uint64_t gdi_session) {
   emit_dirty_user_events(gdi_session);
   emit_dirty_project_events(gdi_session);
}

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

void
ocs::SharetreeUsage::spool_all(uint64_t gdi_session) {
   while (!dirty_users_spool.empty()) {
      spool_one_user(dirty_users_spool.pop_front(), gdi_session);
   }
   while (!dirty_projects_spool.empty()) {
      spool_one_project(dirty_projects_spool.pop_front(), gdi_session);
   }
}

void
ocs::SharetreeUsage::mark_share_tree_dirty() {
   share_tree_dirty = true;
}

bool
ocs::SharetreeUsage::consume_share_tree_dirty() {
   const bool was_dirty = share_tree_dirty;
   share_tree_dirty = false;
   return was_dirty;
}

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
