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
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstring>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull_sort.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_schedd_conf.h"

#include "comm/commlib.h"

#include "sched/schedd_message.h"
#include "sched/sge_schedd_text.h"
#include "sched/sge_orders.h"
#include "sched/msg_schedd.h"

#include "sge_sched_job_category.h"
#include "category.h"

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
 * - int sge_reset_job_category(void)
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


/* Categories of the job are managed here */
static lList *CATEGORY_LIST = nullptr;   /* Category list, which contains the categories referenced
                                       * in the job structure. It is used for the resource matching 
                                       * type = CT_Type
                                       */
static lList *CS_CATEGORY_LIST = nullptr;/* Category list, which contains the categories for the
                                       * category scheduler. The categories are not referenced in
                                       * the job and only used at the beginning, before a scheduling
                                       * run, when the jobs are copied. The flag JC_FILTER=true has
                                       * to be set to make use of it.
                                       */

static bool reb_cat = true;

static bool is_job_pending(lListElem *job);

/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/*    add jobs' category to the global category list, if it doesn't        */
/*    already exist, and reference the category in the job element         */
/*    The category_list is recreated for every scheduler run               */
/*                                                                         */
/*  NOTE: this function is not MT-Safe, because it uses global variables   */
/*                                                                         */
/* SG: TODO: split this into seperate functions                            */
/*-------------------------------------------------------------------------*/
int
sge_add_job_category(lListElem *job, const lList *acl_list, const lList *prj_list, const lList *rqs_list) {

   lListElem *cat = nullptr;
   const char *cstr = nullptr;
   u_long32 rc = 0;
   static const char no_requests[] = "no-requests";
   dstring category_str = DSTRING_INIT;
   bool did_project;

   DENTER(TOP_LAYER);

   /* First part:
      Builds the category for the resource matching
   */

   sge_build_job_category_dstring(&category_str, job, acl_list, prj_list, &did_project, rqs_list);

   if (sge_dstring_strlen(&category_str) == 0) {
      cstr = sge_dstring_copy_string(&category_str, no_requests);
   } else {
      cstr = sge_dstring_get_string(&category_str);
   }

   if (CATEGORY_LIST == nullptr) {
      CATEGORY_LIST = lCreateList("new category list", CT_Type);
   } else {
      cat = lGetElemStrRW(CATEGORY_LIST, CT_str, cstr);
   }

   if (cat == nullptr) {
      cat = lAddElemStr(&CATEGORY_LIST, CT_str, cstr, CT_Type);
   }

   /* increment ref counter and set reference to this element */
   rc = lGetUlong(cat, CT_refcount);
   lSetUlong(cat, CT_refcount, ++rc);
   lSetRef(job, JB_category, cat);

   /* Second part:
      Builds the category for the category scheduler. We need the
      resource category for it. All variables are reused.
   */
   if (sconf_is_job_category_filtering()) {
      lListElem *job_ref = nullptr;
      lList *job_ref_list = nullptr;

      /* 
      ** free category_str
      */
      sge_dstring_clear(&category_str);


      cstr = sge_build_job_cs_category(&category_str, job, cat, did_project);

      cat = nullptr;
      if (cstr == nullptr) {
         cstr = sge_dstring_copy_string(&category_str, no_requests);
      }

      if (CS_CATEGORY_LIST == nullptr) {
         CS_CATEGORY_LIST = lCreateList("category_list", SCT_Type);
      } else {
         cat = lGetElemStrRW(CS_CATEGORY_LIST, SCT_str, cstr);
      }

      if (cat == nullptr) {
         cat = lAddElemStr(&CS_CATEGORY_LIST, SCT_str, cstr, SCT_Type);
         lSetList(cat, SCT_job_pending_ref, lCreateList("pending_jobs", REF_Type));
         lSetList(cat, SCT_job_ref, lCreateList("jobs", REF_Type));
      }

      if (is_job_pending(job)) {
         job_ref_list = lGetListRW(cat, SCT_job_pending_ref);
      } else {
         job_ref_list = lGetListRW(cat, SCT_job_ref);
      }

      job_ref = lCreateElem(REF_Type);
      lSetRef(job_ref, REF_ref, job);
      lAppendElem(job_ref_list, job_ref);

   }

   /* 
   ** free category_str
   */
   sge_dstring_free(&category_str);

   DRETURN(0);
}

/*-------------------------------------------------------------------------*/
/*    delete jobs category if CT_refcount gets 0                          */
/*-------------------------------------------------------------------------*/
int
sge_delete_job_category(lListElem *job) {
   lListElem *cat = nullptr;
   u_long32 rc = 0;

   DENTER(TOP_LAYER);

   /* First part */
   cat = (lListElem *) lGetRef(job, JB_category);
   if (CATEGORY_LIST && cat) {
      rc = lGetUlong(cat, CT_refcount);
      if (rc > 1) {
         lSetUlong(cat, CT_refcount, --rc);
      } else {
         const lListElem *cache = nullptr;
         const lList *cache_list = lGetList(cat, CT_cache);

         DPRINTF(("############## Removing %s from category list (refcount: " sge_u32 ")\n",
                 lGetString(cat, CT_str), lGetUlong(cat, CT_refcount)));

         for_each_ep(cache, cache_list) {
            int *range = (int *) lGetRef(cache, CCT_pe_job_slots);
            sge_free(&range);
         }

         lRemoveElem(CATEGORY_LIST, &cat);
      }
   }
   lSetRef(job, JB_category, nullptr);

   /* Second part */
   /* Removes a job from the category scheduler categories. */
   if (sconf_is_job_category_filtering()) {
      lListElem *ref = nullptr;
      bool found = false;
      int i;
      int max = 2;
      lList *refs[2] = {nullptr, nullptr};
      bool is_job_pending_ = is_job_pending(job);

      for_each_rw(cat, CS_CATEGORY_LIST) {
         if (is_job_pending_) {
            refs[0] = lGetListRW(cat, SCT_job_pending_ref);
            refs[1] = lGetListRW(cat, SCT_job_ref);
         } else {
            refs[0] = lGetListRW(cat, SCT_job_ref);
            refs[1] = lGetListRW(cat, SCT_job_pending_ref);
         }

         for (i = 0; (i < max && !found); i++) {
            for_each_rw(ref, refs[i]) {
               if (lGetRef(ref, REF_ref) == job) {
                  lRemoveElem(refs[i], &ref);
                  found = true;
                  break;
               }
            }

            if (found) { /* is category empty? */
               if ((lGetNumberOfElem(lGetList(cat, SCT_job_pending_ref)) == 0) &&
                   (lGetNumberOfElem(lGetList(cat, SCT_job_ref)) == 0)) {
                  lRemoveElem(CS_CATEGORY_LIST, &cat);
               }
               break;
            }
         }
         if (found) {
            break;
         }
      }
   }

   DRETURN(0);
}

/*-------------------------------------------------------------------------*/


static bool
is_job_pending(lListElem *job) {
   /* SG: TODO:
   this is a very simple evaluation of the job state. It is not accurat
   and should be addopted to the real state model, but for now it is good
   enough to go with the ja_task as an identifier, if we have a pending job
   or not. Jobs, which need to be rescheduled break the accouting here, because
   they have a ja_task and they are pending....

   We have also a problem with array jobs, one of them might be running, but not
   all, which means that there is a JB_ja_tasks list....

   We can do this simple check here, because all we loos is a bit of performance, but
   no wrong decissions will be made later...
 */
   return (lFirst(lGetList(job, JB_ja_tasks)) == nullptr) ? true : false;
}

/*-------------------------------------------------------------------------*/
int
sge_is_job_category_rejected(const lListElem *job) {
   int ret;
   lListElem *cat = nullptr;

   DENTER(TOP_LAYER);
   cat = (lListElem *) lGetRef(job, JB_category);
   ret = sge_is_job_category_rejected_(cat);
   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/
int
sge_is_job_category_reservation_rejected(const lListElem *job) {
   int ret;
   lListElem *cat = nullptr;

   DENTER(TOP_LAYER);
   cat = (lListElem *) lGetRef(job, JB_category);
   ret = sge_is_job_category_reservation_rejected_(cat);
   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/
bool
sge_is_job_category_rejected_(lRef cat) {
   return lGetUlong((lListElem *) cat, CT_rejected) ? true : false;
}

/*-------------------------------------------------------------------------*/
bool
sge_is_job_category_reservation_rejected_(lRef cat) {
   return lGetUlong((lListElem *) cat, CT_reservation_rejected) ? true : false;
}

/*-------------------------------------------------------------------------*/
void
sge_reject_category(lRef cat, bool with_reservation) {
   lSetUlong((lListElem *) cat, CT_rejected, 1);
   if (with_reservation) {
      lSetUlong((lListElem *) cat, CT_reservation_rejected, 1);
   }
}

/*-------------------------------------------------------------------------*/
/* rebuild the category references                                         */
/*-------------------------------------------------------------------------*/
int
sge_rebuild_job_category(const lList *job_list, const lList *acl_list, const lList *prj_list, const lList *rqs_list) {
   lListElem *job;

   DENTER(TOP_LAYER);

   if (!reb_cat) {
      DRETURN(0);
   }

   DPRINTF(("### ### ### ###   REBUILDING CATEGORIES   ### ### ### ###\n"));

   lFreeList(&CATEGORY_LIST);
   lFreeList(&CS_CATEGORY_LIST);

   for_each_rw (job, job_list) {
      sge_add_job_category(job, acl_list, prj_list, rqs_list);
   }

   reb_cat = false;

   DRETURN(0);
}

int
sge_category_count(void) {
   return lGetNumberOfElem(CATEGORY_LIST);
}

int
sge_cs_category_count(void) {
   return lGetNumberOfElem(CS_CATEGORY_LIST);
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
int
sge_reset_job_category() {
   lListElem *cat;
   DENTER(TOP_LAYER);

   for_each_rw (cat, CATEGORY_LIST) {
      const lListElem *cache;

      for_each_ep(cache, lGetList(cat, CT_cache)) {
         int *range = (int *) lGetRef(cache, CCT_pe_job_slots);
         sge_free(&range);
      }

      lSetUlong(cat, CT_rejected, 0);
      lSetInt(cat, CT_count, -1);
      lSetList(cat, CT_cache, nullptr);
      lSetBool(cat, CT_messages_added, false);
      lSetBool(cat, CT_rc_valid, false);
   }

   DRETURN(0);
}

/****** sge_category/sge_category_job_copy() **********************************
*  NAME
*     sge_category_job_copy() -- copies jobs based on the categories and max slots.
*
*  SYNOPSIS
*     int sge_category_job_copy(lList *job_list, lList *queue_list) 
*
*  FUNCTION
*     Copies jobs based on the max nr open slots and the categories. Each
*     category will get max amount of slots pending jobs. Even if there are
*     no open slots available, it will generate at least minJobPerCategory
*     jobs for each category.
*
*  INPUT
*     lList *queue_list = a list with all queue instances.
*     lList **orders = (out) contains a order list to remove priority info
*
*  RESULT
*     int - a reduced job list
*
*  NOTES
*     MT-NOTE: sge_reset_job_category() is not MT safe 
*
*******************************************************************************/
lList *
sge_category_job_copy(lList *queue_list, lList **orders, bool monitor_next_run) {
   const int minJobPerCategory = 5;
   const int maxJobPerCategory = 300;

   lList *jobListCopy = nullptr;
   const lListElem *queue = nullptr;
   const lListElem *category = nullptr;
   int jobPerCategory = 0;

   DENTER(TOP_LAYER);

   INFO("the job category filter is enabled");

   for_each_ep(queue, queue_list) {
      u_long32 state = lGetUlong(queue, QU_state);

      switch (state) {
         case QI_UNKNOWN :
         case QI_ERROR :
         case QI_CAL_SUSPENDED :
         case QI_AMBIGUOUS :
         case QI_ORPHANED :
         case QI_DISABLED :
         case QI_CAL_DISABLED:
            continue;
         default:
            jobPerCategory += (lGetUlong(queue, QU_job_slots) - qinstance_slots_used(queue));
      }
   }

   if (jobPerCategory < minJobPerCategory) {
      jobPerCategory = minJobPerCategory;
   } else if (jobPerCategory > maxJobPerCategory) {
      jobPerCategory = maxJobPerCategory;
   }

   for_each_ep(category, CS_CATEGORY_LIST) {
      const lListElem *job_ref = nullptr;
      int copy_counter = 0;

      /* copy running jobs and others maybe pending */
      for_each_ep(job_ref, lGetList(category, SCT_job_ref)) {
         lListElem *job = (lListElem *) lGetRef(job_ref, REF_ref);
         if (jobListCopy == nullptr) {
            jobListCopy = lCreateListHash("copy_job_list", lGetElemDescr(job), false);
         }
         lAppendElem(jobListCopy, lCopyElem(job));
      }

      /* copy pending jobs, only pending till max is reached */
      for_each_ep(job_ref, lGetList(category, SCT_job_pending_ref)) {
         lListElem *job = (lListElem *) lGetRef(job_ref, REF_ref);

         /* only copy, if we have free slots left, or the jobs needs a reservation */
         if ((copy_counter < jobPerCategory) || (lGetBool(job, JB_reserve))) {

            lListElem *ja_structure = lFirstRW(lGetList(job, JB_ja_structure)); /* get array job size */
            int amount = lGetUlong(ja_structure, RN_max);
            const lList *pe_range = lGetList(job, JB_pe_range); /* get pe requests */

            if (jobListCopy == nullptr) {
               jobListCopy = lCreateListHash("copy_job_list", lGetElemDescr(job), false);
            }

            /*compute array job size */
            if (amount != 1) {
               amount = ((amount - lGetUlong(ja_structure, RN_min)) / lGetUlong(ja_structure, RN_step)) + 1;
            }

            /* compute pe job size (puting the array size into account) */
            if (pe_range != nullptr) {
               const lListElem *pe = lFirst(pe_range);
               amount *= lGetUlong(pe, RN_min);
            }

            lAppendElem(jobListCopy, lCopyElem(job));
            copy_counter += amount;
         } else {
            schedd_mes_add_join(monitor_next_run, lGetUlong(job, JB_job_number), SCHEDD_INFO_JOB_CATEGORY_FILTER_);
            *orders = sge_create_orders(*orders, ORT_clear_pri_info, job, nullptr, nullptr, false);
         }
      }
   }

   schedd_mes_commit(nullptr, false, nullptr);
   cull_hash_create_hashtables(jobListCopy);

   DRETURN(jobListCopy);
}

void
set_rebuild_categories(bool new_value) {
   reb_cat = new_value;
}
