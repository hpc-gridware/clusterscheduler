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
 * @brief Implementation of the suspend threshold handling
 */
#include <cstring>
#include <ctime>

#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_order.h"
#include <utility>

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"

#include "sge_orders.h"
#include "suspend_thresholds.h"


static std::pair<ocs::tom::Element<ocs::tom::JB>, ocs::tom::Element<ocs::tom::JAT>>
select4suspension(ocs::tom::List<ocs::tom::JB> job_list, ocs::tom::ConstElement<ocs::tom::QU> qep);
static std::pair<ocs::tom::Element<ocs::tom::JB>, ocs::tom::Element<ocs::tom::JAT>>
select4unsuspension(ocs::tom::List<ocs::tom::JB> job_list, ocs::tom::ConstElement<ocs::tom::QU> qep);

/**
 * @brief Selects and suspends jobs in queues that exceed their thresholds
 *
 * Per queue at most `QU_nsuspend` jobs are suspended, and only once per
 * `QU_suspend_interval` - a queue whose interval has not expired is skipped.
 * A queue without an interval, without `QU_nsuspend` or without thresholds
 * has the feature disabled.
 *
 * @param[in]     susp_queues queues that are over their suspend thresholds
 * @param[in]     job_list    the jobs, to pick the ones to suspend
 * @param[in,out] orders      the suspend orders are appended here
 */
void suspend_job_in_queues(const ocs::tom::List<ocs::tom::QU> susp_queues, const ocs::tom::List<ocs::tom::JB> job_list,
                           order_t *orders) {
   DENTER(TOP_LAYER);
   int i, found;

   uint64_t now = sge_get_gmt64();
   for (const auto qep : susp_queues) {
      uint32_t interval;

      /* are suspend thresholds enabled? */
      parse_ulong_val(nullptr, &interval, ocs::CEntry::Type::TIME, ocs::tom::get<QU_suspend_interval>(qep), nullptr, 0);

      if (interval == 0
          || !ocs::tom::get<QU_nsuspend>(qep)
          || !ocs::tom::get<QU_suspend_thresholds>(qep)) {
         continue;
      } 

      /* check time stamp */
      if (ocs::tom::get<QU_last_suspend_threshold_ckeck>(qep) &&
         (ocs::tom::get<QU_last_suspend_threshold_ckeck>(qep) + sge_gmt32_to_gmt64(interval) > now)) {
         continue;
      }

      for (i = 0, found = 1; 
           i < (int) ocs::tom::get<QU_nsuspend>(qep) && found; 
           i++) {
         found = 0;
         /* find one running job in suspend queue */
         const auto [jep, ja_task] = select4suspension(job_list, qep);
         if (ja_task == nullptr) {
            break;
         }

         /* generate suspend order for found job */
         found = 1;
         sge_create_orders(orders->configOrderList, ORT_suspend_on_threshold, jep, ja_task,
                           ocs::tom::ConstList<ocs::tom::JG>(), true);

         DPRINTF("++++ suspending job " sge_u32 "/" sge_u32 " on threshold\n",
                 ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(ja_task));

         /* prevent multiple selection of this job */
         ocs::tom::set<JAT_state>(ja_task, 
                   ocs::tom::get<JAT_state>(ja_task) | JSUSPENDED_ON_THRESHOLD);
      }

      if (i==0 && !found) {
         DPRINTF("found no jobs for sot in queue %s\n", ocs::tom::get<QU_full_name>(qep));
      }
   }
   
   DRETURN_VOID;
}


/**
 * @brief Unsuspends jobs in queues that are below their thresholds again
 *
 * The counterpart of suspend_job_in_queues(), with the same per interval and
 * per queue limits.
 *
 * @param[in]     queue_list  the queues to look at
 * @param[in]     job_list    the jobs, to pick the ones to unsuspend
 * @param[in,out] orders      the unsuspend orders are appended here
 */
void unsuspend_job_in_queues(const ocs::tom::List<ocs::tom::QU> queue_list, const ocs::tom::List<ocs::tom::JB> job_list,
                             order_t *orders) {
   DENTER(TOP_LAYER);
   int i, found;

   uint64_t now = sge_get_gmt64();
   for (const auto qep : queue_list) {
      uint32_t interval;

      /* are suspend thresholds enabled? */
      parse_ulong_val(nullptr, &interval, ocs::CEntry::Type::TIME, ocs::tom::get<QU_suspend_interval>(qep), nullptr, 0);

       if (interval == 0
           || !ocs::tom::get<QU_nsuspend>(qep)
           || !ocs::tom::get<QU_suspend_thresholds>(qep)) {
          continue;
       } 

      /* check time stamp */
      if (ocs::tom::get<QU_last_suspend_threshold_ckeck>(qep) &&
         (ocs::tom::get<QU_last_suspend_threshold_ckeck>(qep) + sge_gmt32_to_gmt64(interval) > now)) {
         if (DPRINTF_IS_ACTIVE) {
            DSTRING_STATIC(dstr1, 64);
            DSTRING_STATIC(dstr2, 64);
            DPRINTF("queue was last checked at %s (interval = %s, now = %s)\n",
                    sge_ctime64(ocs::tom::get<QU_last_suspend_threshold_ckeck>(qep), &dstr1),
                    ocs::tom::get<QU_suspend_interval>(qep), sge_ctime64(now, &dstr2));
         }
         continue;
      }

      for (i = 0, found = 1; 
           i < (int) ocs::tom::get<QU_nsuspend>(qep) && found; 
           i++) {
         found = 0;
         /* find one running job in suspend queue */
         const auto [jep, ja_task] = select4unsuspension(job_list, qep);
         if (ja_task == nullptr) {
            break;
         }

         /* generate unsuspend order for found job */
         found = 1;
         sge_create_orders(orders->configOrderList, ORT_unsuspend_on_threshold, jep, ja_task,
                           ocs::tom::ConstList<ocs::tom::JG>(), true);

         DPRINTF("---- unsuspending job " sge_u32 "/" sge_u32 " on threshold\n",
                 ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(ja_task));

         /* prevent multiple selection of this job */
         ocs::tom::set<JAT_state>(ja_task, ocs::tom::get<JAT_state>(ja_task) & ~JSUSPENDED_ON_THRESHOLD);
      }
      
      if (i==0 && !found) {
         DPRINTF("found no jobs for usot in queue %s\n", ocs::tom::get<QU_full_name>(qep));
      }
   }

   DRETURN_VOID;
}


/**
 * @brief Picks the job to suspend next in a queue that exceeds its suspend thresholds
 *
 * Among the running, not yet suspended tasks that use the queue, the one that
 * has run the shortest time is taken.
 *
 * @param job_list the jobs (JB)
 * @param qep      the queue instance over its thresholds (QU)
 *
 * @return the job and the task to suspend, both empty if the queue is suspended already or no task qualifies
 */
static std::pair<ocs::tom::Element<ocs::tom::JB>, ocs::tom::Element<ocs::tom::JAT>>
select4suspension(const ocs::tom::List<ocs::tom::JB> job_list, const ocs::tom::ConstElement<ocs::tom::QU> qep) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::JB> jshortest;
   ocs::tom::Element<ocs::tom::JAT> shortest;

   const char *qnm = ocs::tom::get<QU_full_name>(qep);
   if (qinstance_state_is_manual_suspended(qep) ||
       qinstance_state_is_susp_on_sub(qep) ||
       qinstance_state_is_cal_suspended(qep)) {
      DRETURN(std::make_pair(jshortest, shortest));
   }

   for (const auto jep : job_list) {
      /* job running */ 
      for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(jep)) {
         const uint32_t jstate = ocs::tom::get<JAT_state>(ja_task);
         if (!(jstate & JRUNNING) || 
             (jstate & JSUSPENDED) || (jstate & JSUSPENDED_ON_THRESHOLD)) {
            continue;
         }

         /*
         ** if the current task is
         **    a job / one task of an array-job
         **    a master-task of a pe-job with sub-tasks in this queue
         ** then it is a potential candidate which we could suspend
         */
         if (ocs::tom::find_first_in<JAT_granted_destin_identifier_list, JG_qname>(ja_task, qnm) == nullptr) {
            continue;
         }

         /* select job that runs shortest time for suspension */
         if (shortest == nullptr ||
             // @todo: CS-353 the condition looks incorrect
             ocs::tom::get<JAT_start_time>(shortest) < ocs::tom::get<JAT_start_time>(ja_task)) {
            shortest = ja_task;
            jshortest = jep;
         }
      }
   }

   DRETURN(std::make_pair(jshortest, shortest));
}

/**
 * @brief Picks the job to unsuspend next in a queue that is below its suspend thresholds again
 *
 * Among the tasks suspended on threshold whose master queue is this queue, the
 * one that has run the longest time is taken.
 *
 * @param job_list the jobs (JB)
 * @param qep      the queue instance below its thresholds (QU)
 *
 * @return the job and the task to unsuspend, both empty if no task qualifies
 */
static std::pair<ocs::tom::Element<ocs::tom::JB>, ocs::tom::Element<ocs::tom::JAT>>
select4unsuspension(const ocs::tom::List<ocs::tom::JB> job_list, const ocs::tom::ConstElement<ocs::tom::QU> qep) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::JB> jlongest;
   ocs::tom::Element<ocs::tom::JAT> longest;
   const char *qnm = ocs::tom::get<QU_full_name>(qep);

   for (const auto jep : job_list) {
      for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(jep)) {
         /* job must be suspended */ 
         const uint32_t jstate = ocs::tom::get<JAT_state>(ja_task);
         if (!(jstate & JSUSPENDED_ON_THRESHOLD)) {
            continue;
         }

         /* is this the master queue of this job ? */
         if (strcmp(qnm, ocs::tom::get<JG_qname>(ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task).front()))) {
            DTRACE;
            continue;
         }

         /* select task that runs longest time for unsuspension */
         if (longest == nullptr ||
             ocs::tom::get<JAT_start_time>(longest) > ocs::tom::get<JAT_start_time>(ja_task)) {
            longest = ja_task;
            jlongest = jep;
         }
      }
   }

   DRETURN(std::make_pair(jlongest, longest));
}
