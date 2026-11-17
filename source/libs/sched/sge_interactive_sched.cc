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
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "sgeobj/tom/ocs_tom_RN.h"

#include "sge_orders.h"
#include "sge_interactive_sched.h"

static void
order_remove_order_and_immediate(ocs::tom::ConstElement<ocs::tom::JB> job,
                                 ocs::tom::ConstElement<ocs::tom::JAT> ja_task, order_t *orders);

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
 * @param pending_job_list The list of pending jobs for this scheduler pass
 * @param running_job_list The list of running jobs for this scheduler pass
 * @param orders The order structure for this scheduler pass
 *
 * @return Error code: 0 = OK, 1 = Errors -- always returns 0
 *
 * @note MT-NOTE: remove_immediate_jobs() is MT safe
 */
int remove_immediate_jobs(const ocs::tom::List<ocs::tom::JB> pending_job_list,
                          const ocs::tom::List<ocs::tom::JB> running_job_list, order_t *orders) {
   DENTER(TOP_LAYER);

   auto next_job = pending_job_list.front();
   while (next_job) {
      auto job = next_job;
      next_job = job.next();

      /* skip non immediate .. */
      if (!JOB_TYPE_IS_IMMEDIATE(ocs::tom::get<JB_type>(job))) {
         continue;
      }

      /* .. and non idle jobs */
      auto first_task = ocs::tom::get<JB_ja_tasks>(job).front();
      if (first_task && ocs::tom::get<JAT_status>(first_task) == JIDLE) {
         continue;
      }

      // the job is freed when it is removed, its number is needed for the running list
      auto job_number = ocs::tom::get<JB_job_number>(job);

      /* Remove the job from the pending list */
      remove_immediate_job(pending_job_list, job, orders, 0);

      /* If the job also exists in the running list, we need to remove it there
       * as well since array jobs are all or nothing. */
      auto running_job = ocs::tom::find_first_rw<JB_job_number>(running_job_list, job_number);
      if (running_job) {
         remove_immediate_job(running_job_list, running_job, orders, 1);
      }
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
 * @param job_list The list of jobs from which the job should be removed
 * @param job The job to remove, freed afterwards
 * @param orders The order structure for this scheduler pass
 * @param remove_orders Whether the ORT_start_job orders should also be removed
 *
 * @note MT-NOTE: remove_immediate_job() is MT safe
 */
void remove_immediate_job(const ocs::tom::List<ocs::tom::JB> job_list, const ocs::tom::Element<ocs::tom::JB> job,
                          order_t *orders, const int remove_orders) {
   DENTER(TOP_LAYER);

   for (auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
      if (remove_orders) {
         order_remove_order_and_immediate(job, ja_task, orders);
      }
      else {
         order_remove_immediate(job, ja_task, orders);
      }
   }
   
   for (auto range : ocs::tom::get<JB_ja_n_h_ids>(job)) {
      for (uint32_t ja_task_id = ocs::tom::get<RN_min>(range);
           ja_task_id <= ocs::tom::get<RN_max>(range);
           ja_task_id += ocs::tom::get<RN_step>(range)) {
         auto ja_task = job_get_ja_task_template_pending(job, ja_task_id);

         /* No need to remove the orders here because tasks in JB_ja_n_h_ids
          * haven't been scheduled, and hence don't have start orders to
          * remove. */
         order_remove_immediate(job, ja_task, orders);
      }
   }
   job_list.erase(job);

   DRETURN_VOID;
}

/**
 * @brief Add a remove order for the job task
 *
 * Generates an order of type ORT_remove_immediate_job for the given job
 * task.  Also removes the ORT_start_job order for this task from the order
 * list.
 *
 * @param job The job to remove
 * @param ja_task The task to remove
 * @param orders The order structure for this scheduler pass
 *
 * @note MT-NOTE: order_remove_order_and_immediate() is MT safe
 */
static void
order_remove_order_and_immediate(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                 const ocs::tom::ConstElement<ocs::tom::JAT> ja_task, order_t *orders) {
   DENTER(TOP_LAYER);

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
   const auto order_list = orders->jobStartOrderList.get();
   auto job_number = ocs::tom::get<JB_job_number>(job);
   auto task_number = ocs::tom::get<JAT_task_number>(ja_task);

   // remove the first start order of this task
   for (auto ep : order_list) {
      if (ocs::tom::get<OR_type>(ep) == ORT_start_job && ocs::tom::get<OR_job_number>(ep) == job_number &&
          ocs::tom::get<OR_ja_task_number>(ep) == task_number) {
         DPRINTF("Removing job start order for job task %u.%u\n", job_number, task_number);
         order_list.erase(ep);
         break;
      }
   }

   order_remove_immediate(job, ja_task, orders);

   DRETURN_VOID;
}

/**
 * @brief Add a remove order for the job task
 *
 * Generates an order of type ORT_remove_immediate_job for the given job
 * task.
 *
 * @param job The job to remove
 * @param ja_task The task to remove
 * @param orders The order structure will be extended by one del order
 *
 * @return Error code: 0 = OK, 1 = Errors
 *
 * @note MT-NOTE: order_remove_immediate() is MT safe
 */
int order_remove_immediate(const ocs::tom::ConstElement<ocs::tom::JB> job,
                           const ocs::tom::ConstElement<ocs::tom::JAT> ja_task, order_t *orders) {
   DENTER(TOP_LAYER);

   DPRINTF("JOB " sge_u32 "." sge_u32 " can't get dispatched - removing\n",
           ocs::tom::get<JB_job_number>(job), ocs::tom::get<JAT_task_number>(ja_task));
   
   sge_create_orders(orders->jobStartOrderList, ORT_remove_immediate_job, job, ja_task,
                     ocs::tom::ConstList<ocs::tom::JG>(), true);

   DRETURN((orders->jobStartOrderList == nullptr));
}
