/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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
 * @brief Scheduling categories: deciding once for every job that looks alike
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_dstring.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_Category.h"
#include "sgeobj/ocs_Session.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CCT.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "ocs_CategoryQmaster.h"
#include "ocs_DataStore.h"
#include "sge_event_master.h"
#include "sge_resource_quota_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sge_pe_qmaster.h"
#include "sge_host_qmaster.h"

/******************************************************
 *
 * Description:
 *
 * Categories are used to speed up the job dispatching
 * in the scheduler. Before the job dispatching starts,
 * the categories have to be build for new jobs and
 * reseted for existing jobs. A new job gets a reference
 * to its category regardless if it is existing or not.
 *
 * This is done with:
 * - sge_add_job_category(lListElem *job, lList *acl_list)
 * - sge_rebuild_job_category(lList *job_list, lList *acl_list)
 * - int sge_reset_job_category()
 *
 * During the dispatch run for a job, the category caches
 * all hosts and queues, which are not suitable for that
 * category. This leads toa speed improvement when other
 * jobs of the same category are matched. In addition to
 * the host and queues, it has to cach the generated messages
 * as well, since the are not generated again. If a category
 * cannot run in the cluster at all, the category is rejected
 * and the messages are added for all jobs in the category.
 *
 * This is done for simple and parallel jobs. In addition it
 * also caches the results of soft request matching. Since
 * a job can only soft request a fixed resource, it is not
 * changing during a scheduling run and the soft request violation
 * for a given queue are the same for all jobs in one
 * category.
 *
 ******************************************************/

/** @brief Prepare the projects and user sets the categories are built from
 *
 * Flags every project and user set that is referenced as an access list in a
 * resource quota set, a cluster queue, a parallel environment or an execution
 * host: only those become part of a category string.
 *
 * The access lists of the parallel environments are collected before those of
 * the cluster queues and hosts; only the set of flagged objects matters, not
 * the order of the collected references.
 *
 * @param master_project_list the projects (`PR`)
 * @param master_userset_list the user sets (`US`)
 * @param master_rqs_list the resource quota sets (`RQS`)
 * @param master_cqueue_list the cluster queues (`CQ`)
 * @param master_pe_list the parallel environments (`PE`)
 * @param master_host_list the execution hosts (`EH`)
 */
void
ocs::CategoryQmaster::initialize_prj_and_uset_for_categories(const ocs::tom::List<ocs::tom::PR> master_project_list,
                                                             const ocs::tom::List<ocs::tom::US> master_userset_list,
                                                             const ocs::tom::ConstList<ocs::tom::RQS> master_rqs_list,
                                                             const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                                                             const ocs::tom::ConstList<ocs::tom::PE> master_pe_list,
                                                             const ocs::tom::ConstList<ocs::tom::EH> master_host_list) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::PR> p_list;
   ocs::tom::ListPtr<ocs::tom::US> u_list;
   bool all_projects = false;
   bool all_usersets = false;

   /*
    * collect a list of references to usersets/projects used in
    * the resource quota sets
    */
   for (const auto rqs : master_rqs_list) {
      if (!all_projects && !rqs_diff_projects(rqs, nullptr, &p_list, nullptr, master_project_list)) {
         all_projects = true;
      }
      if (!all_usersets && !rqs_diff_usersets(rqs, nullptr, &u_list, nullptr, master_userset_list)) {
         all_usersets = true;
      }
      if (all_usersets && all_projects) {
         break;
      }
   }

   /*
    * collect list of references to usersets/projects used as ACL
    * with queue_conf(5), host_conf(5) and sge_pe(5)
    */
   for (const auto pe : master_pe_list) {
      pe_diff_usersets(pe, nullptr, &u_list, nullptr);
   }

   for (const auto cq : master_cqueue_list) {
      cqueue_diff_projects(cq, nullptr, &p_list, nullptr);
      cqueue_diff_usersets(cq, nullptr, &u_list, nullptr);
   }

   for (const auto hep : master_host_list) {
      host_diff_projects(hep, nullptr, &p_list, nullptr);
      host_diff_usersets(hep, nullptr, &u_list, nullptr);
   }

   /*
    * now set categories flag with usersets/projects used as ACL
    */
   for (const auto ep : *p_list) {
      const auto prj = master_project_list.find_first<PR_name>(ep.get<PR_name>());
      if (prj) {
         prj.set<PR_consider_with_categories>(true);
      }
   }

   for (const auto ep : *u_list) {
      const auto acl = master_userset_list.find_first<US_name>(ep.get<US_name>());
      if (acl) {
         acl.set<US_consider_with_categories>(true);
      }
   }

   DRETURN_VOID;
}

/** @brief Build the categories for every job at startup
 *
 * Projects and user sets have to be prepared first, because a category is
 * partly defined by the access rights a job has.
 *
 * @param master_category_list location of the categories (`CT`), created if it is still nullptr
 * @param master_job_list the jobs (`JB`)
 * @param master_project_list the projects (`PR`)
 * @param master_userset_list the user sets (`US`)
 * @param master_rqs_list the resource quota sets (`RQS`)
 * @param master_cqueue_list the cluster queues (`CQ`)
 * @param master_pe_list the parallel environments (`PE`)
 * @param master_host_list the execution hosts (`EH`)
 */
void
ocs::CategoryQmaster::initialize_prj_uset_and_create_categories(
        lList **master_category_list, const ocs::tom::List<ocs::tom::JB> master_job_list,
        const ocs::tom::List<ocs::tom::PR> master_project_list, const ocs::tom::List<ocs::tom::US> master_userset_list,
        const ocs::tom::ConstList<ocs::tom::RQS> master_rqs_list,
        const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
        const ocs::tom::ConstList<ocs::tom::PE> master_pe_list,
        const ocs::tom::ConstList<ocs::tom::EH> master_host_list) {
   DENTER(TOP_LAYER);

   // Initialize projects and usersets for the used is categories
   // names will be part of the category string if they are used in cqueue, PEs, hosts or RQS
   initialize_prj_and_uset_for_categories(master_project_list, master_userset_list, master_rqs_list,
                                          master_cqueue_list, master_pe_list, master_host_list);

   // Create all categories
   attach_all_jobs(master_job_list, master_category_list, master_userset_list, master_project_list,
                   master_rqs_list, false, SessionManager::GDI_SESSION_NONE);

   DRETURN_VOID;
}


/** @brief Work out a job's category and attach it
 *
 * @param master_category_list location of the categories (`CT`), created if it is still nullptr
 * @param job the job (`JB`)
 * @param master_userset_list the user sets (`US`)
 * @param master_project_list the projects (`PR`)
 * @param master_rqs_list the resource quota sets (`RQS`)
 * @param send_events whether to announce the category change to the event clients
 * @param gdi_session the session the change belongs to
 * @return true when the job was attached
 */
bool
ocs::CategoryQmaster::attach_job(lList **master_category_list, const ocs::tom::Element<ocs::tom::JB> job,
                                 const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                                 const ocs::tom::ConstList<ocs::tom::PR> master_project_list,
                                 const ocs::tom::ConstList<ocs::tom::RQS> master_rqs_list,
                                 const bool send_events, const uint32_t gdi_session) {
   DENTER(TOP_LAYER);

   // check if the input parameters are valid
   if (master_category_list == nullptr || !job) {
      DRETURN(false);
   }

   // check if the category list is already created
   if (*master_category_list == nullptr) {
      // the master list slot of the data store is untyped
      *master_category_list = ocs::tom::ListPtr<ocs::tom::CT>::create("master category list").release();
   }
   const auto category_list = ocs::tom::List<ocs::tom::CT>::from_typed(*master_category_list);

   // build the category string
   dstring category_str = DSTRING_INIT;
   Category::build_string(&category_str, job, master_userset_list, master_project_list, master_rqs_list);
   const char *cat_str = sge_dstring_get_string(&category_str);

   // get the category or create a new one
   bool is_new = false;
   auto category = category_list.find_first<CT_str>(cat_str);
   if (!category) {
      category = category_list.emplace_back<CT_str>(cat_str);
      category.set<CT_id>(Category::get_next_id(category_list));
      is_new = true;
   }
   sge_dstring_free(&category_str);

   // Increase the reference count
   category.set<CT_refcount>(category.get<CT_refcount>() + 1);

   // Point to the category in the job
   const uint32_t category_id = category.get<CT_id>();
   job.set<JB_category_id>(category_id);

   // Send events if required
   if (send_events) {
      ev_event category_event_type = is_new ? sgeE_CATEGORY_ADD : sgeE_CATEGORY_MOD;
      sge_add_event(0, category_event_type, category_id, 0, nullptr,
                    nullptr, nullptr, category, gdi_session);
   }

   DRETURN(true);
}


/** @brief Detach a job from its category, dropping the category when it empties
 *
 * @param master_category_list location of the categories (`CT`)
 * @param job the job (`JB`)
 * @param send_events whether to announce the category change to the event clients
 * @param gdi_session the session the change belongs to
 * @return true when the job was detached
 */
bool
ocs::CategoryQmaster::detach_job(lList **master_category_list, const ocs::tom::Element<ocs::tom::JB> job,
                                 const bool send_events, const uint32_t gdi_session) {
   DENTER(TOP_LAYER);

   // check if the input parameters are valid
   if (master_category_list == nullptr && !job) {
      DRETURN(false);
   }
   const auto category_list = ocs::tom::List<ocs::tom::CT>::from_typed(*master_category_list);
   auto category = category_list.find_first<CT_id>(job.get<JB_category_id>());

   // each category that is referenced in a job should also exist
   SGE_ASSERT(category);

   job.set<JB_category_id>(0);

   // decrease the reference count or remove the category
   bool is_del = false;
   uint32_t refcount = category.get<CT_refcount>();
   uint32_t category_id = category.get<CT_id>();
   if (refcount > 1) {
      category.set<CT_refcount>(refcount - 1);
   } else {
      category_list.erase(category);
      // as lRemoveElem() did: the event of a removed category carries no element
      category = nullptr;
      is_del = true;
   }

   if (send_events) {
      ev_event category_event = is_del ? sgeE_CATEGORY_DEL : sgeE_CATEGORY_MOD;
      sge_add_event(0, category_event, category_id, 0,
                    nullptr, nullptr, nullptr, category, gdi_session);
   }

   DRETURN(true);
}


/** @brief Recompute a job's category and move it if it changed
 *
 * @param master_category_list location of the categories (`CT`)
 * @param job the job (`JB`)
 * @param master_userset_list the user sets (`US`)
 * @param master_project_list the projects (`PR`)
 * @param master_rqs_list the resource quota sets (`RQS`)
 * @param send_events whether to announce the category change to the event clients
 * @param now the current time
 * @param gdi_session the session the change belongs to
 */
void
ocs::CategoryQmaster::reattach_job(lList **master_category_list, const ocs::tom::Element<ocs::tom::JB> job,
                                   const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                                   const ocs::tom::ConstList<ocs::tom::PR> master_project_list,
                                   const ocs::tom::ConstList<ocs::tom::RQS> master_rqs_list,
                                   const bool send_events, const uint64_t now, const uint32_t gdi_session) {
   DENTER(TOP_LAYER);

   // remove the job from current category
   detach_job(master_category_list, job, send_events, gdi_session);

   // add the job to the new category
   attach_job(master_category_list, job, master_userset_list, master_project_list, master_rqs_list, send_events,
              gdi_session);

   const uint32_t job_number = job.get<JB_job_number>();
   sge_add_event(now, sgeE_JOB_MOD, job_number, 0, nullptr, nullptr, nullptr, job, gdi_session);
   DRETURN_VOID;
}

/** @brief Attach every job to a category
 *
 * @param master_job_list the jobs (`JB`)
 * @param master_category_list location of the categories (`CT`), created if it is still nullptr
 * @param master_userset_list the user sets (`US`)
 * @param master_project_list the projects (`PR`)
 * @param master_rqs_list the resource quota sets (`RQS`)
 * @param send_events whether to announce the category change to the event clients
 * @param gdi_session the session the change belongs to
 */
void
ocs::CategoryQmaster::attach_all_jobs(const ocs::tom::List<ocs::tom::JB> master_job_list, lList **master_category_list,
                                      const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                                      const ocs::tom::ConstList<ocs::tom::PR> master_project_list,
                                      const ocs::tom::ConstList<ocs::tom::RQS> master_rqs_list,
                                      const bool send_events, const uint32_t gdi_session) {
   DENTER(TOP_LAYER);

   // add all jobs to the category list, create categories if they do not exist
   for (const auto job : master_job_list) {
      attach_job(master_category_list, job, master_userset_list, master_project_list, master_rqs_list, send_events,
                 gdi_session);
   }
   DRETURN_VOID;
}

/** @brief Recompute every job's category
 *
 * Needed after a change to anything a category depends on - a project, a
 * user set, a resource quota or a queue.
 *
 * @param master_job_list the jobs (`JB`)
 * @param master_userset_list the user sets (`US`)
 * @param master_project_list the projects (`PR`)
 * @param master_rqs_list the resource quota sets (`RQS`)
 * @param send_events whether to announce the category change to the event clients
 * @param gdi_session the session the change belongs to
 */
void
ocs::CategoryQmaster::reattach_all_jobs(const ocs::tom::List<ocs::tom::JB> master_job_list,
                                        const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                                        const ocs::tom::ConstList<ocs::tom::PR> master_project_list,
                                        const ocs::tom::ConstList<ocs::tom::RQS> master_rqs_list,
                                        const bool send_events, const uint32_t gdi_session) {
   DENTER(TOP_LAYER);
   lList **master_category_list = DataStore::get_master_list_rw(SGE_TYPE_CATEGORY);
   const uint64_t now = sge_get_gmt64();

   for (const auto job : master_job_list) {
      reattach_job(master_category_list, job, master_userset_list, master_project_list, master_rqs_list, send_events,
                   now, gdi_session);
   }
   DRETURN_VOID;
}


/**
 * @brief Resets the category temp information
 *
 * Some information in the category should only life throu one scheduling run.
 * These informations are reseted in the call:
 * - dispatching messages
 * - soft violations
 * - not suitable cluster
 * - the flag that identifies, if the messages are already added to the schedd infos
 * - something with the resource reservation
 *
 * @param master_category_list the categories to reset (`CT`)
 *
 * @note MT-NOTE: sge_reset_job_category() is not MT safe
 */
void
ocs::CategoryQmaster::reset_tmp_data(const ocs::tom::List<ocs::tom::CT> master_category_list) {
   DENTER(TOP_LAYER);

   for (const auto cat : master_category_list) {

      // deallocate memory stored in the cache itself
      for (ocs::tom::Element<ocs::tom::CCT> cache : cat.get<CT_cache>()) {
         auto *range = static_cast<int *>(cache.get<CCT_pe_job_slots>());
         sge_free(&range);
         cache.set<CCT_pe_job_slots>(nullptr);
      }

      // reset the cache and the messages added flag
      cat.set<CT_cache>(nullptr);
      cat.set<CT_messages_added>(false);

      // reset information if category was rejected
      cat.set<CT_rejected>(false);
      cat.set<CT_reservation_rejected>(false);

      // reset cached resource contribution
      cat.set<CT_rc_valid>(false);
      cat.set<CT_resource_contribution>(0.0);
   }

   DRETURN_VOID;
}

/** @brief Copy the category's cached decisions back into the job
 *
 * @param master_category_list the categories (`CT`)
 * @param job the job (`JB`)
 */
void
ocs::CategoryQmaster::refresh_cat_data_in_job(const ocs::tom::List<ocs::tom::CT> master_category_list,
                                              const ocs::tom::Element<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   const uint32_t category_id = job.get<JB_category_id>();
   const auto category = master_category_list.find_first<CT_id>(category_id);

   SGE_ASSERT(category);

   job.set<JB_category>(category);
   DRETURN_VOID;
}

/** @brief Refresh the cached category data in every job
 *
 * @param master_category_list the categories (`CT`)
 * @param master_job_list the jobs (`JB`)
 */
void
ocs::CategoryQmaster::refresh_cat_data_all_jobs(const ocs::tom::List<ocs::tom::CT> master_category_list,
                                                const ocs::tom::List<ocs::tom::JB> master_job_list) {
   DENTER(TOP_LAYER);

   if (!master_category_list || !master_job_list) {
      DRETURN_VOID;
   }

   for (const auto job : master_job_list) {
      refresh_cat_data_in_job(master_category_list, job);
   }
   DRETURN_VOID;
}
