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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
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


/* Categories of the job are managed here */
static lList *CATEGORY_LIST = nullptr;   /* Category list, which contains the categories referenced
                                       * in the job structure. It is used for the resource matching 
                                       * type = CT_Type
                                       */

static bool reb_cat = true;

/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/*    add jobs' category to the global category list, if it doesn't        */
/*    already exist, and reference the category in the job element         */
/*                                                                         */
/*  NOTE: this function is not MT-Safe, because it uses global variables   */
/*                                                                         */
/* SG: TODO: split this into separate functions                            */
/*-------------------------------------------------------------------------*/
int
sge_add_job_category(lListElem *job, const lList *acl_list, const lList *prj_list, const lList *rqs_list) {
   DENTER(TOP_LAYER);

   static const char no_requests[] = "no-requests";
   dstring category_str = DSTRING_INIT;
   bool did_project;

   /* First part:
      Builds the category for the resource matching
   */
   sge_build_job_category_dstring(&category_str, job, acl_list, prj_list, &did_project, rqs_list);

   const char *cstr;
   if (sge_dstring_strlen(&category_str) == 0) {
      cstr = sge_dstring_copy_string(&category_str, no_requests);
   } else {
      cstr = sge_dstring_get_string(&category_str);
   }

   lListElem *cat = lGetElemStrRW(CATEGORY_LIST, CT_str, cstr);
   if (cat == nullptr) {
      cat = lAddElemStr(&CATEGORY_LIST, CT_str, cstr, CT_Type);
   }

   // increment ref counter and set reference to this element
   u_long32 rc = lGetUlong(cat, CT_refcount);
   lSetUlong(cat, CT_refcount, ++rc);
   lSetRef(job, JB_category, cat);

   // free category string
   sge_dstring_free(&category_str);

   DRETURN(0);
}

/*-------------------------------------------------------------------------*/
/*    delete jobs category if CT_refcount gets 0                          */
/*-------------------------------------------------------------------------*/
int
sge_delete_job_category(lListElem *job) {
   DENTER(TOP_LAYER);

   /* First part */
   auto *cat = static_cast<lListElem *>(lGetRef(job, JB_category));
   if (CATEGORY_LIST != nullptr && cat != nullptr) {
      u_long32 rc = lGetUlong(cat, CT_refcount);
      if (rc > 1) {
         lSetUlong(cat, CT_refcount, --rc);
      } else {
         const lListElem *cache = nullptr;
         const lList *cache_list = lGetList(cat, CT_cache);

         DPRINTF("############## Removing %s from category list (refcount: " sge_u32 ")\n",
                 lGetString(cat, CT_str), lGetUlong(cat, CT_refcount));

         for_each_ep(cache, cache_list) {
            auto range = static_cast<int *>(lGetRef(cache, CCT_pe_job_slots));
            sge_free(&range);
         }

         lRemoveElem(CATEGORY_LIST, &cat);
      }
   }
   lSetRef(job, JB_category, nullptr);

   DRETURN(0);
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
   return lGetBool((lListElem *) cat, CT_rejected);
}

/*-------------------------------------------------------------------------*/
bool
sge_is_job_category_reservation_rejected_(lRef cat) {
   return lGetBool((lListElem *) cat, CT_reservation_rejected);
}

/*-------------------------------------------------------------------------*/
void
sge_reject_category(lRef cat, bool with_reservation) {
   lSetBool((lListElem *) cat, CT_rejected, true);
   if (with_reservation) {
      lSetBool((lListElem *) cat, CT_reservation_rejected, true);
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

   DPRINTF("### ### ### ###   REBUILDING CATEGORIES   ### ### ### ###\n");

   lFreeList(&CATEGORY_LIST);

   for_each_rw (job, job_list) {
      sge_add_job_category(job, acl_list, prj_list, rqs_list);
   }

   reb_cat = false;

   DRETURN(0);
}

int
sge_category_count() {
   return lGetNumberOfElem(CATEGORY_LIST);
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
   DENTER(TOP_LAYER);

   lListElem *cat;
   for_each_rw (cat, CATEGORY_LIST) {
      // deallocate memory stored in the cache itself
      lListElem *cache;
      for_each_rw(cache, lGetList(cat, CT_cache)) {
         auto *range = static_cast<int *>(lGetRef(cache, CCT_pe_job_slots));
         sge_free(&range);
         lSetRef(cache, CCT_pe_job_slots, nullptr);
      }

      // now assignment (@todo make it boolean in master branch)
      lSetBool(cat, CT_rejected, false);

      // reservation assignment (@todo make it boolean in master branch)
      lSetBool(cat, CT_reservation_rejected, false);

      // @todo remove in master branch. This field is unused
      lSetInt(cat, CT_count, -1);

      // reset the cache and the messages added flag
      lSetList(cat, CT_cache, nullptr);
      lSetBool(cat, CT_messages_added, false);

      // reset cached resource contribution
      lSetBool(cat, CT_rc_valid, false);
      lSetDouble(cat, CT_resource_contribution, 0.0);
   }

   DRETURN(0);
}

void
set_rebuild_categories(bool new_value) {
   reb_cat = new_value;
}
