/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_dstring.h"

#include "sgeobj/ocs_Category.h"
#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_job.h"

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

void
ocs::CategoryQmaster::initialize_prj_and_uset_for_categories(lList *master_project_list, lList *master_userset_list,
                                                             const lList *master_rqs_list, const lList *master_cqueue_list,
                                                             const lList *master_pe_list, const lList *master_host_list) {
   const lListElem *cq, *pe, *hep, *ep;
   const lListElem *rqs;
   lList *u_list = nullptr, *p_list = nullptr;
   bool all_projects = false;
   bool all_usersets = false;

   /*
    * collect a list of references to usersets/projects used in
    * the resource quota sets
    */
   for_each_ep(rqs, master_rqs_list) {
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
   for_each_ep(cq, master_cqueue_list) {
      cqueue_diff_projects(cq, nullptr, &p_list, nullptr);
      cqueue_diff_usersets(cq, nullptr, &u_list, nullptr);
   }

   for_each_ep(pe, master_pe_list) {
      pe_diff_usersets(pe, nullptr, &u_list, nullptr);
   }

   for_each_ep(hep, master_host_list) {
      host_diff_projects(hep, nullptr, &p_list, nullptr);
      host_diff_usersets(hep, nullptr, &u_list, nullptr);
   }

   /*
    * now set categories flag with usersets/projects used as ACL
    */
   for_each_ep(ep, p_list) {
      lListElem *prj = lGetElemStrRW(master_project_list, PR_name, lGetString(ep, PR_name));
      if (prj != nullptr) {
         lSetBool(prj, PR_consider_with_categories, true);
      }
   }

   for_each_ep(ep, u_list) {
      lListElem *acl = lGetElemStrRW(master_userset_list, US_name, lGetString(ep, US_name));
      if (acl != nullptr) {
         lSetBool(acl, US_consider_with_categories, true);
      }
   }

   lFreeList(&p_list);
   lFreeList(&u_list);
}

void
ocs::CategoryQmaster::initialize_prj_uset_and_create_categories(lList **master_category_list, lList *master_job_list,
                                                                lList *master_project_list, lList *master_userset_list,
                                                                const lList *master_rqs_list, const lList *master_cqueue_list,
                                                                const lList *master_pe_list, const lList *master_host_list) {
   // Initialize projects and usersets for the used is categories
   // names will be part of the category string if they are used in cqueue, PEs, hosts or RQS
   initialize_prj_and_uset_for_categories(master_project_list, master_userset_list, master_rqs_list,
                                          master_cqueue_list, master_pe_list, master_host_list);

   // Create all categories
   attach_all_jobs(master_job_list, master_category_list, master_userset_list, master_project_list,
                   master_rqs_list, false, SessionManager::GDI_SESSION_NONE);
}

bool
ocs::CategoryQmaster::attach_job(lList **master_category_list, lListElem *job,
                                 const lList *master_userset_list, const lList *master_project_list, const lList *master_rqs_list,
                                 bool send_events, u_long32 gdi_session) {
   DENTER(TOP_LAYER);

   // check if the input parameters are valid
   if (master_category_list == nullptr || job == nullptr) {
      DRETURN(false);
   }

   // check if the category list is already created
   if (*master_category_list == nullptr) {
      *master_category_list = lCreateList("master category list", CT_Type);
   }

   // build the category string
   dstring category_str = DSTRING_INIT;
   Category::build_string(&category_str, job, master_userset_list, master_project_list, master_rqs_list);
   const char *cat_str = sge_dstring_get_string(&category_str);

   DPRINTF("category string: %s\n", cat_str);

   // get the category or create a new one
   bool is_new = false;
   lListElem *category = lGetElemStrRW(*master_category_list, CT_str, cat_str);
   if (category == nullptr) {
      category = lAddElemStr(master_category_list, CT_str, cat_str, CT_Type);
      lSetUlong(category, CT_id, Category::get_next_id(*master_category_list));
      is_new = true;
   }
   sge_dstring_free(&category_str);

   // Increase the reference count
   lSetUlong(category, CT_refcount, lGetUlong(category, CT_refcount) + 1);

   // Point to the category in the job
   u_long32 category_id = lGetUlong(category, CT_id);
   lSetUlong(job, JB_category_id, category_id);

   // Send events if required
   if (send_events) {
      ev_event category_event_type = is_new ? sgeE_CATEGORY_ADD : sgeE_CATEGORY_MOD;
      sge_add_event(0, category_event_type, category_id, 0, nullptr,
                    nullptr, nullptr, category, gdi_session);
   }

   DRETURN(true);
}

bool
ocs::CategoryQmaster::detach_job(lList **master_category_list, lListElem *job, bool send_events, u_long32 gdi_session) {
   DENTER(TOP_LAYER);

   // check if the input parameters are valid
   if (master_category_list == nullptr && job == nullptr) {
      DRETURN(false);
   }
   lListElem *category = lGetElemUlongRW(*master_category_list, CT_id, lGetUlong(job, JB_category_id));

   // each category that is referenced in a job should also exist
   SGE_ASSERT(category != nullptr);

   // decrease the reference count or remove the category
   bool is_del = false;
   u_long32 refcount = lGetUlong(category, CT_refcount);
   if (refcount > 1) {
      lSetUlong(category, CT_refcount, refcount - 1);
   } else {
      lRemoveElem(*master_category_list, &category);
      is_del = true;
   }

   if (send_events) {
      ev_event category_event = is_del ? sgeE_CATEGORY_DEL : sgeE_CATEGORY_MOD;
      sge_add_event(0, category_event, lGetUlong(job, JB_category_id), 0,
                    nullptr, nullptr, nullptr, category, gdi_session);
   }

   DRETURN(true);
}

void
ocs::CategoryQmaster::reattach_job(lList **master_category_list, lListElem *job,
                                   const lList *master_userset_list, const lList *master_project_list, const lList *master_rqs_list,
                                   bool send_events, u_long32 gdi_session) {
   DENTER(TOP_LAYER);

   // remove the job from current category
   detach_job(master_category_list, job, send_events, gdi_session);

   // add the job to the new category
   attach_job(master_category_list, job, master_userset_list, master_project_list, master_rqs_list, send_events, gdi_session);
   DRETURN_VOID;
}

void
ocs::CategoryQmaster::attach_all_jobs(lList *master_job_list, lList **master_category_list,
                                      const lList *master_userset_list, const lList *master_project_list, const lList *master_rqs_list,
                                      bool send_events, u_long32 gdi_session) {
   DENTER(TOP_LAYER);

   // add all jobs to the category list, create categories if they do not exist
   lListElem *job;
   for_each_rw(job, master_job_list) {
      attach_job(master_category_list, job, master_userset_list, master_project_list, master_rqs_list, send_events, gdi_session);
   }
   DRETURN_VOID;
}

void
ocs::CategoryQmaster::reattach_all_jobs(lList *master_job_list,
                                        const lList *master_userset_list, const lList *master_project_list, const lList *master_rqs_list,
                                        bool send_events, u_long32 gdi_session) {
   DENTER(TOP_LAYER);
   lList **master_category_list = DataStore::get_master_list_rw(SGE_TYPE_CATEGORY);

   lListElem *job;
   for_each_rw(job, master_job_list) {
      reattach_job(master_category_list, job, master_userset_list, master_project_list, master_rqs_list, send_events, gdi_session);
   }
   DRETURN_VOID;
}

/****** sge_category/sge_reset_job_category() **********************************
*  NAME
*     sge_reset_job_category() -- resets the category temp information
*
*  SYNOPSIS
*     int sge_reset_job_category()
*
*  FUNCTION
*     Some information in the category should only life throu one scheduling run.
*     These informations are reseted in the call:
*     - dispatching messages
*     - soft violations
*     - not suitable cluster
*     - the flag that identifies, if the messages are already added to the schedd infos
*     - something with the resource reservation
*
*  RESULT
*     int - always 0
*
*  NOTES
*     MT-NOTE: sge_reset_job_category() is not MT safe
*
*******************************************************************************/
void
ocs::CategoryQmaster::reset_tmp_data(lList *master_category_list) {
   DENTER(TOP_LAYER);

   lListElem *cat;
   for_each_rw (cat, master_category_list) {

      // deallocate memory stored in the cache itself
      lListElem *cache;
      for_each_rw (cache, lGetList(cat, CT_cache)) {
         auto *range = static_cast<int *>(lGetRef(cache, CCT_pe_job_slots));
         sge_free(&range);
         lSetRef(cache, CCT_pe_job_slots, nullptr);
      }

      // reset the cache and the messages added flag
      lSetList(cat, CT_cache, nullptr);
      lSetBool(cat, CT_messages_added, false);

      // reset information if category was rejected
      lSetBool(cat, CT_rejected, false);
      lSetBool(cat, CT_reservation_rejected, false);

      // reset cached resource contribution
      lSetBool(cat, CT_rc_valid, false);
      lSetDouble(cat, CT_resource_contribution, 0.0);
   }

   DRETURN_VOID;
}

void
ocs::CategoryQmaster::refresh_cat_data_in_job(lList *master_category_list, lListElem *job) {
   DENTER(TOP_LAYER);
   u_long32 category_id = lGetUlong(job, JB_category_id);
   lListElem *category = lGetElemUlongRW(master_category_list, CT_id, category_id);

   DPRINTF("###### category id: %lu (%p)\n", category_id, category);

   lSetRef(job, JB_category, category);
   DRETURN_VOID;
}

void
ocs::CategoryQmaster::refresh_cat_data_all_jobs(lList *master_category_list, lList *master_job_list) {
   DENTER(TOP_LAYER);

   if (master_category_list == nullptr || master_job_list == nullptr) {
      DRETURN_VOID;
   }

   lListElem *job;
   for_each_rw(job, master_job_list) {
      refresh_cat_data_in_job(master_category_list, job);
   }
   DRETURN_VOID;
}
