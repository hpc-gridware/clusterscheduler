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
 *  Copyright: 2007 by Rising Sun Pictures
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Array task dependencies
 */

#include <cstdio>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_answer.h"

#include "sge_task_depend.h"
#include "sge_job_qmaster.h"

static uint32_t task_depend_div_floor(uint32_t a, uint32_t b) {
   return a / b;
}


static uint32_t nearest_index_in_A(uint32_t i, uint32_t t0, uint32_t sa) {
   return t0 + task_depend_div_floor(i - t0, sa) * sa;
}


static void task_depend(uint32_t *lb, uint32_t *ub, uint32_t t0, uint32_t sa, uint32_t sb, uint32_t step_b_id) {
   /* simulate the equation i = t0 + n * sb, n in {0, 1, ..., N-1}. */

   uint32_t i = step_b_id;

   *lb = nearest_index_in_A(i, t0, sa);
   *ub = nearest_index_in_A(i + sb - 1, t0, sa);
}


/**
 * @brief Get predecessor job task depdendencies
 *
 * This function determines the range of sub-tasks of job pre_jep that
 * suc_jep.task_id will be dependent on when suc_jep has an explicit
 * array dependency hold on pre_jep (with -hold_jid_ad option).
 *
 * @param range RN_Type pointer
 * @param alpp AN_Type list pointer
 * @param pre_jep const JB_Type element
 * @param suc_jep const JB_Type element
 * @param task_id a valid suc_jep task id
 *
 * @return 0 on success
 *
 * @note Let div(a, b) = floor(a / b)
 *       Let nearest_index_in_A(i) = t0 + div(i - t0, sa) * sa
 *
 *       Sub-task B.i will be dependent on all tasks in A between
 *       nearest_index_in_A(i) and nearest_index_in_A(i + sb - 1) where
 *       i = t0 + n * sb and n is a positive integer.
 *
 *       It is safe to swap pre_jep and suc_jep provided that the given task id
 *       belongs to pre_jep (therefore reversing the sense of the dependence).
 *
 *       MT-NOTE: sge_task_depend_get_range() is MT safe
 */
int
sge_task_depend_get_range(lListElem **range, lList **alpp, const lListElem *pre_jep, const lListElem *suc_jep,
                          uint32_t task_id) {
   uint32_t a0, a1, b0, b1, sa, sb, rmin, rmax;

   DENTER(TOP_LAYER);

   if (range == nullptr ||
       pre_jep == nullptr ||
       suc_jep == nullptr) {
      DRETURN(STATUS_EUNKNOWN);
   }

   job_get_submit_task_ids(pre_jep, &a0, &a1, &sa);
   job_get_submit_task_ids(suc_jep, &b0, &b1, &sb);

   /* sanity check on task ranges */
   if (!sge_task_depend_is_same_range(pre_jep, suc_jep)) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /* make sure task_id is the first task in a range */
   if (task_id < b0 || ((task_id - b0) % sb) != 0) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /* make the actual call to the core dependence function */
   task_depend(&rmin, &rmax, a0, sa, sb, task_id);

   /* do some basic checks on the output */
   if (rmin < a0 || rmax > a1) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /* if an existing range was not given, create one */
   if (*range == nullptr) {
      *range = lCreateElem(RN_Type);
      if (*range == nullptr) {
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   range_set_all_ids(*range, rmin, rmax, sa);

   DRETURN(0);
}

/*****************************************************************************
 Tasks matching this profile are considered finished for dependence purposes
 *****************************************************************************/
static bool
task_depend_is_finished(const lListElem *job, uint32_t task_id) {
   const lListElem *ja_task = nullptr;

   DENTER(TOP_LAYER);

   if (!job_is_enrolled(job, task_id)) {
      DRETURN(false);
   }

   ja_task = job_search_task(job, nullptr, task_id);
   if (ja_task != nullptr) {
      if (lGetUlong(ja_task, JAT_status) != JFINISHED) {
         DRETURN(false);
      }
      for_each_ep_lv(task, lGetList(ja_task, JAT_task_list)) {
         if (lGetUlong(lFirst(lGetList(task, JB_ja_tasks)), JAT_status)
             != JFINISHED) {
            DRETURN(false);
         }
      }
   }

   DRETURN(true);
}


/**
 * @brief Update job array dependencies for a task
 *
 * This function recalculates array dependency hold information
 * for a particular task id of job jep (-hold_jid_ad option).
 * If the task is independent (i.e., has no predecessor tasks that
 * are in an unfinished state), its id will be removed from the
 * JB_ja_a_h_ids hold range and potentially moved to JB_ja_n_h_ids.
 * Enrolled tasks may be held or unheld if they are not finished,
 * causing task mod events to be generated if the task was updated.
 *
 * @param jep JB_Type element
 * @param alpp AN_Type list pointer
 * @param task_id a task id in job jep
 *
 * @return true if the job was modified, otherwise false
 *
 * @note If array dependency information cannot be determined, then
 *       this function will assume that a task dependence still exists.
 *
 *       A false result from this function should not be considered failure.
 *       Update status is returned to help the caller decide whether modify
 *       event code should be emitted.
 *
 *       If the job argument jep is nullptr, or the task indicated by task_id
 *       in jep is finished, false is returned.
 *
 *       MT-NOTE: Is not thread safe. Reads from the global Job-List
 */
bool
sge_task_depend_update(lListElem *jep, lList **alpp, uint32_t task_id, uint64_t gdi_request) {
   DENTER(TOP_LAYER);
   uint32_t hold_state, new_state;
   int Depend = 0;
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   /* this should not really be necessary */
   if (jep == nullptr) {
      DPRINTF("got nullptr for job argument\n");
      DRETURN(false);
   }

   /* JA: FIXME: now emulating qalter -h; just hold the task even if
      it is running, which handles task failed followed by qmod -c */
   if (task_depend_is_finished(jep, task_id)) {
      DRETURN(false);
   }

   /* process the resolved predecessor list */
   for_each_ep_lv(pre, lGetList(jep, JB_ja_ad_predecessor_list)) {
      uint32_t sa, sa_task_id, amin, amax;
      const lListElem *pred_jep = nullptr; /* JB_Type */
      lListElem *dep_range = nullptr;      /* RN_Type */

      /* locate the job id in the master list, if not found we can't do much */
      pred_jep = lGetElemUlong(master_job_list, JB_job_number, lGetUlong(pre, JRE_job_number));
      if (!pred_jep) continue;

      /* use the RSP functions to determine dependent predecessor task range */
      if (sge_task_depend_get_range(&dep_range, alpp, pred_jep, jep, task_id)) {
         /* since we can't calculate it, we must assume dependence */
         lFreeElem(&dep_range);
         Depend = 1;
         break;
      }

      /* fetch predecessor job dependency range ids */
      range_get_all_ids(dep_range, &amin, &amax, &sa);

      /* all tasks between {amin, ..., amax} are dependencies unless finished */
      for (sa_task_id = amin; sa_task_id <= amax; sa_task_id += sa) {
         /* if the task is not finished => dependence */
         if (!task_depend_is_finished(pred_jep, sa_task_id))
            Depend = 1;
      }

      /* cleanup, if a dep range was allocated */
      lFreeElem(&dep_range);

      /* minor speed optimization */
      if (Depend) break;
   }

   /* alter the hold state based on dependence info */
   hold_state = job_get_hold_state(jep, task_id);
   if (Depend) {
      new_state = hold_state | MINUS_H_TGT_JA_AD;
   } else {
      new_state = hold_state & ~MINUS_H_TGT_JA_AD;
   }

   /* update the hold state, possibly moving the task between n_h_ids and
      a_h_ids (or maybe update JAT_hold and JAT_state for enrolled tasks) */
   if (new_state != hold_state) {
      job_set_hold_state(jep, alpp, task_id, new_state);
      if (job_is_enrolled(jep, task_id)) {
         /* all task mod events will need to be added individually */
         lListElem *ja_task = job_search_task(jep, nullptr, task_id);
         if (ja_task != nullptr)
            sge_add_jatask_event(sgeE_JATASK_MOD, jep, ja_task, gdi_request);
         DRETURN(false);
      }
      DRETURN(true);
   }

   DRETURN(false);
}

/**
 * @brief Initialize job array task dependencies
 *
 * This function inits the JB_ja_a_h_ids dependence cache when the
 * array dependency request list is non-empty (-hold_jid_ad option).
 * It might also update the JHELD flag of the JAT_state field for enrolled
 * tasks, placing or clearing the MINUS_H_TGT_JA_AD bits of the JAT_hold
 * field. Task mod events are generated for the modified enrolled tasks.
 *
 * @param jep JB_Type element
 * @param alpp AN_Type list pointer
 *
 * @return true if the job was modified, otherwise false
 *
 * @note Use thus function to completely recalculate array dependencies when
 *       the ja_ad_predecessors list is modified, or upon qmaster restart.
 *
 *       If the job was never an array successor, this function has no effect.
 *
 *       MT-NOTE: sge_task_depend_init() is not MT safe (calls MT unsafe method)
 */
bool
sge_task_depend_init(lListElem *jep, lList **alpp, uint64_t gdi_session) {
   bool ret = false;

   DENTER(TOP_LAYER);

   if (jep == nullptr) {
      DPRINTF("got nullptr for job argument\n");
      DRETURN(false);
   }

   if (lGetNumberOfElem(lGetList(jep, JB_ja_ad_request_list)) > 0) {
      if (lGetNumberOfElem(lGetList(jep, JB_ja_ad_predecessor_list)) == 0) {
         /* fast case where all predecessors are "gone" */
         if (sge_task_depend_flush(jep, alpp, gdi_session))
            ret = true;
      } else {
         uint32_t taskid, b0, b1, sb;
         job_get_submit_task_ids(jep, &b0, &b1, &sb);
         for (taskid = b0; taskid <= b1; taskid += sb) {
            if (sge_task_depend_update(jep, alpp, taskid, gdi_session))
               ret = true;
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Flush job array task dependencies
 *
 * This function clears the JB_ja_a_h_ids dependence cache when it
 * contains one or more task ranges and the job array dependency
 * predecessor list is empty (-hold_jid_ad option). It might also
 * release the JHELD flag of the JAT_state field for enrolled tasks
 * with MINUS_H_TGT_JA_AD in the JAT_hold field. Task mod events will
 * be generated if the enrolled task was updated.
 *
 * @param jep JB_Type element
 * @param alpp AN_Type list pointer
 *
 * @return true if the job was modified, otherwise false
 *
 * @note It is useful to call this function when jobs are removed from the
 *       JB_ja_ad_predecessor_list as a way of ensuring that dependence state
 *       information in both structures is consistent. It is more efficient
 *       to call this function than to update dependency information accross
 *       the entire range of sub-tasks of job jep.
 *
 *       If the job was never an array successor, this function has no effect.
 *
 *       MT-NOTE: sge_task_depend_flush() is MT safe
 */
bool
sge_task_depend_flush(lListElem *jep, lList **alpp, uint64_t gdi_session) {
   bool ret = false;

   DENTER(TOP_LAYER);

   /* this should not really be necessary */
   if (jep == nullptr) {
      DPRINTF("got nullptr for job argument\n");
      DRETURN(false);
   }

   /* ensure empty hold states are consistent. if the request list is empty
      then we don't want to modify array predecessors cache */
   if (lGetNumberOfElem(lGetList(jep, JB_ja_ad_request_list)) > 0 &&
       lGetNumberOfElem(lGetList(jep, JB_ja_ad_predecessor_list)) == 0) {
      if (lGetList(jep, JB_ja_a_h_ids)) {
         lList *a_h_ids = lCopyList("", lGetList(jep, JB_ja_a_h_ids));
         for_each_ep_lv(range, a_h_ids) {
            uint32_t rmin, rmax, rstep, hold_state;
            range_get_all_ids(range, &rmin, &rmax, &rstep);
            for (; rmin <= rmax; rmin += rstep) {
               hold_state = job_get_hold_state(jep, rmin);
               hold_state &= ~MINUS_H_TGT_JA_AD;
               job_set_hold_state(jep, alpp, rmin, hold_state);
            }
         }
         lFreeList(&a_h_ids);
         /* just make sure it is null */
         if (lGetList(jep, JB_ja_a_h_ids)) {
            a_h_ids = nullptr;
            lXchgList(jep, JB_ja_a_h_ids, &a_h_ids);
            lFreeList(&a_h_ids);
         }
         ret = true;
      }
      /* unhold any arary held tasks that are enrolled */
      for_each_rw_lv(ja_task, lGetList(jep, JB_ja_tasks)) {
         uint32_t task_id = lGetUlong(ja_task, JAT_task_number);
         uint32_t hold_state = job_get_hold_state(jep, task_id);
         if ((hold_state & MINUS_H_TGT_JA_AD) != 0) {
            hold_state &= ~MINUS_H_TGT_JA_AD;
            job_set_hold_state(jep, alpp, task_id, hold_state);
            sge_add_jatask_event(sgeE_JATASK_MOD, jep, ja_task, gdi_session);
         }
      }
   }

   DRETURN(ret);
}


/**
 * @brief Determine array job equivalence
 *
 * This function determines if the job arguments are suitable predecessor
 * and successor jobs for an array dependency pair (-hold_jid_ad option).
 *
 * @param pre_jep const JB_Type element
 * @param suc_jep const JB_Type element
 *
 * @return true if jobs are compatible array jobs, otherwise false sge_task_depend_is_same_range() is MT safe
 *
 * @note MT-NOTE
 */
bool
sge_task_depend_is_same_range(const lListElem *pre_jep, const lListElem *suc_jep) {
   uint32_t a0, a1, b0, b1, sa, sb;

   DENTER(TOP_LAYER);

   /* equivalent jobs cannot be nullptr */
   if (pre_jep == nullptr || suc_jep == nullptr) {
      DPRINTF("got nullptr pre_jep or suc_jep job argument\n");
      DRETURN(false);
   }

   /* equivalent jobs must be array jobs (-t option) */
   if (!job_is_array(pre_jep) || !job_is_array(suc_jep)) {
      DRETURN(false);
   }

   /* fetch job submit ranges */
   job_get_submit_task_ids(pre_jep, &a0, &a1, &sa);
   job_get_submit_task_ids(suc_jep, &b0, &b1, &sb);

   /* equivalent jobs must have the same range of sub-tasks */
   if (a0 != b0 || a1 != b1) {
      DRETURN(false);
   }

   DRETURN(true);
}
