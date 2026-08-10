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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Immediate jobs - the ones that must run now or not at all
 *
 * A job submitted with `-now yes` (`qrsh`, `qlogin`, an immediate `qsub`) is
 * not queued: if the scheduler cannot dispatch it in this run, it has to be
 * removed and the submitter told so. This module produces those removal
 * orders.
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_job.h"

#include "sge_orders.h"
#include "sge_interactive_sched.h"

static void 
order_remove_order_and_immediate(const lListElem *job, const lListElem *ja_task,
                                 order_t *orders);

/**
 * @brief Test for and remove immediate jobs which can't
 *
 * Goes through all jobs in the pending list to see if any are immediate and
 * not idle.  If any are, they are removed.  This is done by generating an
 * order of type ORT_remove_immediate_job.  If any array jobs are removed,
 * the running list is checked for tasks belonging to the job, which are
 * also removed.  This is done by removing the ORT_start_job orders and
 * adding an order of type ORT_remove_immediate_job.
 *
 * @param pending_job_list The list of pending jobs for this scheduler pass (JB_Type)
 * @param running_job_list The list of running jobs for this scheduler pass (JB_Type)
 * @param orders The order structure for this scheduler pass
 *
 * @return Error code: 0 = OK, 1 = Errors -- always returns 0
 *
 * @note MT-NOTE: remove_immediate_jobs() is MT safe
 */
int remove_immediate_jobs(lList *pending_job_list, lList *running_job_list, order_t *orders) 
{
   lListElem *next_job, *job;
   const lListElem *ep; 
   const lList* lp;

   DENTER(TOP_LAYER);

   next_job = lFirstRW(pending_job_list);
   while ((job = next_job)) {
      lCondition *where = nullptr;
      next_job = lNextRW(job);
      
      /* skip non immediate .. */
      if (!JOB_TYPE_IS_IMMEDIATE(lGetUlong(job, JB_type))) {
         continue;
      }

      /* .. and non idle jobs */
      if ((lp = lGetList(job, JB_ja_tasks)) && (ep = lFirst(lp)) && lGetUlong(ep, JAT_status)==JIDLE) {
         continue;    
      }

      /* Prepare search condition for running list */
      where = lWhere("%T(%I==%u)", JB_Type, JB_job_number, lGetUlong(job, JB_job_number));
      
      /* Remove the job from the pending list */
      remove_immediate_job(pending_job_list, job, orders, 0);
      
      /* If the job also exists in the running list, we need to remove it there
       * as well since array jobs are all or nothing. */
      if ((job = lFindFirstRW(running_job_list, where)) != nullptr) {
         remove_immediate_job(running_job_list, job, orders, 1);
      }
      
      lFreeWhere(&where);
   }

   DRETURN(0);
}

/**
 * @brief Test for and remove immediate job which can't
 *
 * Removes immediate jobs which cannot be scheduled from the given job list.
 * This is done by generating an order of type ORT_remove_immediate_job.  If
 * remove_orders is set, the ORT_start_job orders are first removed from the
 * order list before adding the remove order.
 *
 * @param job_list The list of jobs from which the job should be removed (JB_Type)
 * @param job The job to remove (JB_Type)
 * @param orders The order structure for this scheduler pass
 * @param remove_orders Whether the ORT_start_job orders should also be be removed
 *
 * @note MT-NOTE: remove_immediate_job() is MT safe
 */
void 
remove_immediate_job(lList *job_list, lListElem *job, order_t *orders, int remove_orders) 
{
   const lList *range_list = nullptr;
   uint32_t ja_task_id;

   DENTER(TOP_LAYER);

   for_each_ep_lv(ja_task, lGetList(job, JB_ja_tasks)) {
      if (remove_orders) {
         order_remove_order_and_immediate(job, ja_task, orders);
      }
      else {
         order_remove_immediate(job, ja_task, orders);
      }
   }
   
   range_list = lGetList(job, JB_ja_n_h_ids);
   
   for_each_ep_lv(range, range_list) {
      for(ja_task_id = lGetUlong(range, RN_min);
          ja_task_id <= lGetUlong(range, RN_max);
          ja_task_id += lGetUlong(range, RN_step)) {  
         lListElem *ja_task = job_get_ja_task_template_pending(job, ja_task_id);
         
         /* No need to remove the orders here because tasks in JB_ja_n_h_ids
          * haven't been scheduled, and hence don't have start orders to
          * remove. */
         order_remove_immediate(job, ja_task, orders);
      }
   }
   lRemoveElem(job_list, &job);
   
   DRETURN_VOID;
}

/**
 * @brief Add a remove order for the job task
 *
 * Generates an order of type ORT_remove_immediate_job for the given job
 * task.  Also removes the ORT_start_job order for this task from the order
 * list.
 *
 * @param job The job to remove  (JB_Type)
 * @param ja_task The task to remove (JAT_Type)
 * @param orders The order structurestructure  for this scheduler pass be removed
 *
 * @note int - Error code: 0 = OK, 1 = Errors
 *
 * @note MT-NOTE: order_remove_order_and_immediate() is MT safe
 */
static void 
order_remove_order_and_immediate(const lListElem *job, const lListElem *ja_task, order_t *orders)
{
   /* The possibility exists that this task is part of an array task, that it
    * already has earned an order to be scheduled, and that one or more other
    * tasks in this same job were not scheduled, resulting in this delete
    * order.  In this case, we have to remove the schedule order before we add
    * the delete order.  Otherwise, the qmaster will think we're trying to do
    * an ORT_remove_immediate_job on a non-idle task. */   

   /* Warning: we have a problem, if we send start orders during the dispatch run. We
    * might have started a array task of an immediate array job without verifying, that
    * the whole job can run
    */  
   lList *orderList = orders->jobStartOrderList;
   lCondition *where = lWhere("%T(%I==%u && %I==%u && %I==%u)", OR_Type,
                      OR_type, ORT_start_job,
                      OR_job_number, lGetUlong(job, JB_job_number),
                      OR_ja_task_number, lGetUlong(ja_task, JAT_task_number));
   lListElem *ep = lFindFirstRW(orderList, where);
   
   DENTER(TOP_LAYER);
   
   if (ep != nullptr) {
      DPRINTF("Removing job start order for job task %u.%u\n", lGetUlong(job, JB_job_number), lGetUlong(ja_task, JAT_task_number));
      lRemoveElem(orderList, &ep);
   }
   
   order_remove_immediate(job, ja_task, orders);
   lFreeWhere(&where);
   
   DRETURN_VOID;
}

/**
 * @brief Add a remove order for the job task
 *
 * Generates an order of type ORT_remove_immediate_job for the given job
 * task.
 *
 * @param job The job to remove (JB_Type)
 * @param ja_task The task to remove (JAT_Type)
 * @param orders The order structure will be extended by one del order
 *
 * @return Error code: 0 = OK, 1 = Errors
 *
 * @note MT-NOTE: order_remove_immediate() is MT safe
 */
int 
order_remove_immediate(const lListElem *job, const lListElem *ja_task, order_t *orders)
{
   DENTER(TOP_LAYER);

   DPRINTF("JOB " sge_u32 "." sge_u32 " can't get dispatched - removing\n",
           lGetUlong(job, JB_job_number), lGetUlong(ja_task, JAT_task_number));
   
   orders->jobStartOrderList = sge_create_orders(orders->jobStartOrderList, ORT_remove_immediate_job,
                                                 job, ja_task, nullptr, true);
   
   DRETURN((orders->jobStartOrderList == nullptr));
}
