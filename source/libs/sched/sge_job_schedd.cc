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
 * @brief Splitting the job list into the categories a scheduling run needs
 *
 * A scheduling run is only interested in the jobs that could actually start.
 * split_jobs() walks the job list **once** and distributes the jobs over an
 * array of result lists indexed by the SPLIT_* values - pending, running,
 * suspended, in error, in hold, waiting for a predecessor, waiting for their
 * start time. Everything that is not a candidate is then thrown away by
 * trash_splitted_jobs(), after a scheduling message has been produced for it.
 */
#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "cull/cull_hash.h"

#include "sgeobj/sge_range.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "uti/sge_stdlib.h"

#include "sge_job_schedd.h"
#include "schedd_monitor.h"
#include "schedd_message.h"
#include "sge_schedd_text.h"

#include "msg_schedd.h"

/** Value of a job counter entry that means "no jobs of this user are running" */
#define IDLE 0

/**
 * @brief Determine a jobs runtime duration
 *
 * The minimum of the time values the user specified with -l h_rt=`time`
 * and -l s_rt=`time` is returned in 'duration'. If neither of these
 * time values were specified the default duration is used.
 *
 * @param duration Returns duration on success
 * @param jep The job (JB_Type)
 *
 * @return true on success
 *
 * @note MT-NOTE: job_get_duration() is MT safe
 */
bool job_get_duration(uint64_t *duration, const lListElem *jep) {
   DENTER(TOP_LAYER);

   if (!job_get_wallclock_limit(duration, jep)) {
      *duration = sge_gmt32_to_gmt64(sconf_get_default_duration());
   }

   DRETURN(true);
}

/**
 * @brief Determin tasks effective runtime limit
 *
 * Determines the effictive runtime limit got by requested h_rt/s_rt or
 * by the resulting queues h_rt/s_rt
 *
 * @param duration tasks duration in seconds
 * @param ja_task task element
 *
 * @return true
 *
 * @note MT-NOTE: task_get_duration() is MT safe
 */
bool task_get_duration(uint64_t *duration, const lListElem *ja_task) {
   DENTER(TOP_LAYER);

   if (ja_task != nullptr) {
      *duration = lGetUlong64(ja_task, JAT_wallclock_limit);
      if (*duration == std::numeric_limits<uint64_t>::max()) {
         *duration = sge_gmt32_to_gmt64(sconf_get_default_duration());
      }
   } else {
      *duration = sge_gmt32_to_gmt64(sconf_get_default_duration());
   }

   DRETURN(true);
}

/**
 * @brief Constant to name transformation
 *
 * This function transforms a constant value in its internal
 * name. (Used for debug output)
 *
 * @param value SPLIT_-Constant
 *
 * @return string representation of 'value'
 */
const char *get_name_of_split_value(int value) {
   const char *name;
   switch (value) {
   case SPLIT_FINISHED:
      name = "SPLIT_FINISHED"; 
      break;
   case SPLIT_WAITING_DUE_TO_PREDECESSOR:
      name = "SPLIT_WAITING_DUE_TO_PREDECESSOR";
      break;
   case SPLIT_HOLD:
      name = "SPLIT_HOLD";
      break;
   case SPLIT_ERROR:
      name = "SPLIT_ERROR";
      break;
   case SPLIT_WAITING_DUE_TO_TIME:
      name = "SPLIT_WAITING_DUE_TO_TIME";
      break;
   case SPLIT_RUNNING:
      name = "SPLIT_RUNNING";
      break;
   case SPLIT_PENDING:
      name = "SPLIT_PENDING";
      break;
   case SPLIT_PENDING_EXCLUDED:
      name = "SPLIT_PENDING_EXCLUDED";
      break;
   case SPLIT_SUSPENDED:
      name = "SPLIT_SUSPENDED";
      break;
   case SPLIT_PENDING_EXCLUDED_INSTANCES:
      name = "SPLIT_PENDING_EXCLUDED_INSTANCES";
      break;   
   default:
      name = "undefined";
      break;
   }
   return name;
}

/**
 * @brief Moves a job from the pending list to the running list
 *
 * Move the 'pending_job' from 'splitted_jobs[SPLIT_PENDING]'
 * into 'splitted_jobs[SPLIT_RUNNING]'. If 'pending_job' is an
 * array job, than the first task (task id) will be moved into
 * 'pending_job[SPLIT_RUNNING]'
 *
 * @param[in,out] pending_job    the pending job (`JB_Type`); set to nullptr
 *                               when the whole job was moved
 * @param[in,out] splitted_jobs  the array of job lists, indexed by the
 *                               SPLIT_* values
 *
 * @return true if the pending job was removed
 *
 * @see #split_jobs
 */
bool job_move_first_pending_to_running(lListElem **pending_job, lList **splitted_jobs[]) {
   DENTER(TOP_LAYER);

   bool ret = false;
   lList *ja_task_list = nullptr;      /* JAT_Type */
   lList *r_ja_task_list = nullptr;    /* JAT_Type */
   lListElem *ja_task = nullptr;       /* JAT_Type */
   lListElem *running_job = nullptr;   /* JB_Type */
   uint32_t job_id;
   uint32_t ja_task_id;

   job_id = lGetUlong(*pending_job, JB_job_number);
   ja_task_list = lGetListRW(*pending_job, JB_ja_tasks);
   ja_task = lFirstRW(ja_task_list);
   
   /*
    * Create list for running jobs
    */
   if (*(splitted_jobs[SPLIT_RUNNING]) == nullptr) {
      const lDescr *descriptor = lGetElemDescr(*pending_job);
      *(splitted_jobs[SPLIT_RUNNING]) = lCreateList("", descriptor);
   } else {
      running_job = lGetElemUlongRW(*(splitted_jobs[SPLIT_RUNNING]), JB_job_number, job_id);
   }
   /*
    * Create a running job if it does not exist aleady 
    */
   if (running_job == nullptr) {
      lList *n_h_ids = nullptr;
      lList *u_h_ids = nullptr;
      lList *o_h_ids = nullptr;
      lList *s_h_ids = nullptr;
      lList *a_h_ids = nullptr;
      lList *r_tasks = nullptr;
      
      lXchgList(*pending_job, JB_ja_n_h_ids, &n_h_ids);
      lXchgList(*pending_job, JB_ja_u_h_ids, &u_h_ids);
      lXchgList(*pending_job, JB_ja_o_h_ids, &o_h_ids);
      lXchgList(*pending_job, JB_ja_s_h_ids, &s_h_ids);
      lXchgList(*pending_job, JB_ja_a_h_ids, &a_h_ids);
      lXchgList(*pending_job, JB_ja_tasks, &r_tasks);
      running_job = lCopyElem(*pending_job);
      lXchgList(*pending_job, JB_ja_n_h_ids, &n_h_ids);
      lXchgList(*pending_job, JB_ja_u_h_ids, &u_h_ids);
      lXchgList(*pending_job, JB_ja_o_h_ids, &o_h_ids);
      lXchgList(*pending_job, JB_ja_s_h_ids, &s_h_ids);
      lXchgList(*pending_job, JB_ja_a_h_ids, &a_h_ids);
      lXchgList(*pending_job, JB_ja_tasks, &r_tasks);
      lAppendElem(*(splitted_jobs[SPLIT_RUNNING]), running_job); 
   } 

   /* 
    * Create an array instance and add it to the running job
    * or move the existing array task into the running job 
    */
   if (ja_task == nullptr) {
      const lList *n_h_ids = lGetList(*pending_job, JB_ja_n_h_ids);

      ja_task_id = range_list_get_first_id(n_h_ids, nullptr);
      ja_task = job_search_task(*pending_job, nullptr, ja_task_id);
      /* JG: TODO: do we need the ja_task instance here or can we
       *           wait until the JATASK_ADD event arrives from qmaster?
       *           The function should work on a copy if the job list.
       *           The event from qmaster has effect on the mirrored lists.
       *           So the code should be ok.
       */
      if (ja_task == nullptr) {
         ja_task = job_create_task(*pending_job, nullptr, ja_task_id);
      }
      ja_task_list = lGetListRW(*pending_job, JB_ja_tasks);
   }
  
   /*
    * Create an array task list if necessary
    */
   r_ja_task_list = lGetListRW(running_job, JB_ja_tasks); 
   if (r_ja_task_list == nullptr) {
      r_ja_task_list = lCreateList("", lGetElemDescr(ja_task));
      lSetList(running_job, JB_ja_tasks, r_ja_task_list);
   }
  
   lDechainElem(ja_task_list, ja_task);
   lAppendElem(r_ja_task_list, ja_task); 
   
   /*
    * Remove pending job if there are no pending tasks anymore
    */
   if (job_count_pending_tasks(*pending_job, false)==0) {
      lDechainElem(*(splitted_jobs[SPLIT_PENDING]), *pending_job);
      lFreeElem(pending_job);
      ret = true;
   }

#if 0 /* EB: DEBUG */
   job_lists_print(splitted_jobs);
#endif

   DRETURN(ret);
}

/**
 * @brief Returns the next task of a job that is a candidate for dispatching
 *
 * Prefers an enrolled task; if the job has none, the first not enrolled id of
 * `JB_ja_n_h_ids` is taken and a pending task template is built for it.
 *
 * @param[in]  job      the job to take the task from
 * @param[out] task_ret receives the task
 * @param[out] id_ret   receives the id of the task
 *
 * @return 0 on success, -1 if the job has no task left
 */
int job_get_next_task(lListElem *job, lListElem **task_ret, uint32_t *id_ret) {
   DENTER(TOP_LAYER);

   lListElem *ja_task;
   uint32_t ja_task_id;

   ja_task = lFirstRW(lGetList(job, JB_ja_tasks));
   if (ja_task == nullptr) {
      lList *answer_list = nullptr;

      ja_task_id = range_list_get_first_id(lGetList(job, JB_ja_n_h_ids), &answer_list);
      if (answer_list_has_error(&answer_list)) {
         lFreeList(&answer_list);
         DRETURN(-1);
      }
      ja_task = job_get_ja_task_template_pending(job, ja_task_id);
   } else {
      ja_task_id = lGetUlong(ja_task, JAT_task_number);
   }

   *task_ret = ja_task;
   *id_ret   = ja_task_id;

   DRETURN(0);
}


/**
 * @brief Inc. the # of jobs a user has running
 *
 * Initialize "user_list" and JC_jobs attribute for each user according
 * to the list of running jobs.
 *
 * @param[in,out] user_list           the job counters per user (`JC_Type`)
 * @param[in]     splitted_job_lists   the array of job lists, indexed by the
 *                                     SPLIT_* values; the running and the
 *                                     suspended list are counted
 */
void user_list_init_jc(lList **user_list, lList **splitted_job_lists[]) {
   if (splitted_job_lists[SPLIT_RUNNING] != nullptr) {
      for_each_ep_lv(job, *(splitted_job_lists[SPLIT_RUNNING])) {
         // @todo (CS-451) the 3rd argument to sge_inc_jc is "slots", but we pass the number of array tasks. Correct?
         sge_inc_jc(user_list, lGetString(job, JB_owner), job_get_ja_tasks(job));
      }
   }
   if (splitted_job_lists[SPLIT_SUSPENDED] != nullptr) {
      for_each_ep_lv(job, *(splitted_job_lists[SPLIT_SUSPENDED])) {
         sge_inc_jc(user_list, lGetString(job, JB_owner), job_get_ja_tasks(job));
      }
   }
}

/**
 * @brief Moves the jobs that would exceed the per user job limit
 *
 * Every pending job beyond `max_jobs_per_user` is moved into
 * `job_lists[SPLIT_PENDING_EXCLUDED]` and gets a scheduling message saying
 * why.
 *
 * @param[in]     monitor_next_run  whether the messages also go into the
 *                                  scheduler run log
 * @param[in,out] job_lists         the array of job lists, indexed by the
 *                                  SPLIT_* values
 * @param[in,out] user_list         the job counters per user (`JC_Type`)
 * @param[in]     user_name         the user to limit, or nullptr for all users
 * @param[in]     max_jobs_per_user the limit, the `max_u_jobs` of the
 *                                  scheduler configuration
 *
 * @note JC_jobs of the user elements contained in "user_list" has to be
 *       initialized properly before this function might be called.
 *
 * @see #trash_splitted_jobs, #split_jobs, #user_list_init_jc
 */
void job_lists_split_with_reference_to_max_running(bool monitor_next_run, lList **job_lists[],
                                                   lList **user_list,
                                                   const char *user_name,
                                                   uint32_t max_jobs_per_user) {
   DENTER(TOP_LAYER);
   if (max_jobs_per_user != 0 && 
       job_lists[SPLIT_PENDING] != nullptr &&
       *(job_lists[SPLIT_PENDING]) != nullptr &&
       job_lists[SPLIT_PENDING_EXCLUDED] != nullptr) {
      lListElem *user = nullptr;
      lListElem *next_user = nullptr;
#ifndef CULL_NO_HASH
      /* 
       * create a hash table on JB_owner to speedup 
       * searching for jobs of a specific owner
       */
      cull_hash_new_check(*(job_lists[SPLIT_PENDING]), JB_owner, false);
#endif      

      if (user_name == nullptr) {
         next_user = lFirstRW(*user_list);
      } else {
         next_user = lGetElemStrRW(*user_list, JC_name, user_name);
      }
      while ((user = next_user) != nullptr) {
         uint32_t jobs_for_user = lGetUlong(user, JC_jobs);
         const char *jc_user_name = lGetString(user, JC_name);

         if (user_name == nullptr) {
            next_user = lNextRW(user);
         } else {
            next_user = nullptr;
         }
         if (jobs_for_user >= max_jobs_per_user) {
            const void *user_iterator = nullptr;
            lListElem *user_job = nullptr;         /* JB_Type */
            lListElem *next_user_job = nullptr;    /* JB_Type */

            DPRINTF("USER %s reached limit of %d jobs\n", jc_user_name, max_jobs_per_user);
            next_user_job = lGetElemStrFirstRW(*(job_lists[SPLIT_PENDING]), 
                                             JB_owner, jc_user_name, 
                                             &user_iterator);
            while ((user_job = next_user_job)) {
               next_user_job = lGetElemStrNextRW(*(job_lists[SPLIT_PENDING]), 
                                               JB_owner, jc_user_name, 
                                               &user_iterator);
               schedd_mes_add(nullptr, monitor_next_run,
                              lGetUlong(user_job, JB_job_number),
                              SCHEDD_INFO_USRGRPLIMIT_);

               lDechainElem(*(job_lists[SPLIT_PENDING]), user_job);
               if (*(job_lists[SPLIT_PENDING_EXCLUDED]) == nullptr) {
                  lDescr *descr = user_job->descr;
                  int pos = lGetPosInDescr(descr, JB_owner);
        
                  if (pos >= 0) {
                     if (descr[pos].ht != nullptr)  {
                        sge_free(&(descr[pos].ht));
                     }
                  }
                  *(job_lists[SPLIT_PENDING_EXCLUDED]) =
                                      lCreateList("", descr);
               }

               lAppendElem(*(job_lists[SPLIT_PENDING_EXCLUDED]), user_job);
            }
         }
      } 
   }
   DRETURN_VOID;
}

/**
 * @brief Split list of jobs according to their state
 *
 * Split a list of jobs according to their state.
 * 'job_list' is the input list of jobs. The jobs in this list
 * have different job states. For the dispatch algorithm only
 * those jobs are of interest which are really pending. Jobs
 * which are pending and in error state or jobs which have a
 * hold applied (start time in future, administrator hold, ...)
 * are not necessary for the dispatch algorithm.
 * After a call to this function the jobs of 'job_list' may
 * have been moved into one of the 'result_list's.
 * Each of those lists containes jobs which have a certain state.
 * (e.g. result_list[SPLIT_WAITING_DUE_TO_TIME] will contain
 * all jobs which have to wait according to their start time.
 * 'max_aj_instances' are the maximum number of tasks of an
 * array job which may be instantiated at the same time.
 * 'max_aj_instances' is used for the split decitions.
 * In case of any error the 'answer_list' will be used to report
 * errors (It is not used in the moment)
 *
 * @param[in,out] job_list         the input list of jobs (`JB_Type`); the
 *                                 jobs are moved out of it unless `do_copy`
 * @param[in]     max_aj_instances maximum number of tasks of an array job
 *                                 that may be instantiated at the same time
 * @param[out]    result_list      the array of result lists (`JB_Type`),
 *                                 indexed by the SPLIT_* values
 * @param[in]     do_copy          true to copy the jobs instead of moving
 *                                 them, leaving `job_list` intact
 *
 * @note In former versions of SGE/EE we had 8 split functions.
 *       Each of those functions walked twice over the job list.
 *       This was time consuming in case of x thousand of jobs.
 *
 *       We tried to improve this:
 *       - loop over all jobs only once
 *       - minimize copy operations where possible
 *
 *       Unfortunately this function is heavy to understand now. Sorry!
 *
 * @see #trash_splitted_jobs, #job_lists_split_with_reference_to_max_running
 */
void split_jobs(lList **job_list, uint32_t max_aj_instances,
                lList **result_list[], bool do_copy) {
   DENTER(TOP_LAYER);

#if 0 /* EB: DEBUG: enable debug messages for split_jobs() */
#define JOB_SPLIT_DEBUG
#endif
   lListElem *job = nullptr;
   lListElem *next_job = nullptr;
   lListElem *previous_job = nullptr;
   next_job = lFirstRW(*job_list);
   while ((job = next_job)) {
      lList *ja_task_list = nullptr;
      lList *n_h_ids = nullptr;
      lList *excluded_n_h_ids = nullptr;
      lList *u_h_ids = nullptr;
      lList *o_h_ids = nullptr;
      lList *s_h_ids = nullptr;
      lList *a_h_ids = nullptr;
      lList *target_tasks[SPLIT_LAST];
      lListElem *target_job[SPLIT_LAST];
      lList *target_ids = nullptr;
      int target_for_ids = SPLIT_LAST;
      lListElem *ja_task = nullptr;
      lListElem *next_ja_task = nullptr;
      uint32_t task_instances;
      int i, move_job;
#ifdef JOB_SPLIT_DEBUG
      uint32_t job_id = lGetUlong(job, JB_job_number);
#endif

      previous_job = lPrevRW(job);
      next_job = lNextRW(job);

      /*
       * Initialize
       */
      for (i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
         target_job[i] = nullptr;
         target_tasks[i] = nullptr;
      }
      task_instances = lGetNumberOfElem(lGetList(job, JB_ja_tasks)); 

      /*
       * Remove all ballast for a minimal copy operation of the job
       */
      lXchgList(job, JB_ja_tasks, &ja_task_list);
      lXchgList(job, JB_ja_n_h_ids, &n_h_ids);
      lXchgList(job, JB_ja_u_h_ids, &u_h_ids);
      lXchgList(job, JB_ja_o_h_ids, &o_h_ids);
      lXchgList(job, JB_ja_s_h_ids, &s_h_ids);
      lXchgList(job, JB_ja_a_h_ids, &a_h_ids);

      /*
       * Split enrolled tasks
       */
#ifdef JOB_SPLIT_DEBUG
      DPRINTF("Split enrolled tasks for job " sge_u32 ":\n", job_id);
#endif
      next_ja_task = lFirstRW(ja_task_list);
      while ((ja_task = next_ja_task)) {
         uint32_t ja_task_status = lGetUlong(ja_task, JAT_status);
         uint32_t ja_task_state = lGetUlong(ja_task, JAT_state);
         uint32_t ja_task_hold = lGetUlong(ja_task, JAT_hold);
         lList **target = nullptr;
#ifdef JOB_SPLIT_DEBUG
         uint32_t ja_task_id = lGetUlong(ja_task, JAT_task_number);
#endif
         next_ja_task = lNextRW(ja_task);

#ifdef JOB_SPLIT_DEBUG
         DPRINTF(("Task " sge_u32 ": status=" sge_u32 " state=" sge_u32"\n", ja_task_id, ja_task_status, ja_task_state));
#endif

         /*
          * Check the state of the task
          * (ORDER IS IMPORTANT!)
          */
         if (target == nullptr && result_list[SPLIT_DEFERRED] &&
             (ja_task_status & JDEFERRED_REQ)) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is in deferred state\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_DEFERRED]);
         } 

         if (target == nullptr && result_list[SPLIT_FINISHED] &&
             (ja_task_status & JFINISHED)) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is in finished state\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_FINISHED]);
         } 

         if (target == nullptr && result_list[SPLIT_ERROR] &&
             (ja_task_state & JERROR)) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is in error state\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_ERROR]);
         } 
         if (target == nullptr && result_list[SPLIT_WAITING_DUE_TO_TIME] &&
             (lGetUlong64(job, JB_execution_time) > sge_get_gmt64()) &&
             (ja_task_status == JIDLE)) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is waiting due to time.\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_WAITING_DUE_TO_TIME]);
         }
         if (target == nullptr && result_list[SPLIT_WAITING_DUE_TO_PREDECESSOR] &&
             (lGetList(job, JB_jid_predecessor_list) != nullptr) &&
             (ja_task_status == JIDLE)) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is waiting due to pred.\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_WAITING_DUE_TO_PREDECESSOR]);
         }
         if (target == nullptr && result_list[SPLIT_PENDING] &&
             (ja_task_status == JIDLE) &&
             !(ja_task_hold & MINUS_H_TGT_ALL)) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is in pending state\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_PENDING]);
         } 
         if (target == nullptr && result_list[SPLIT_SUSPENDED]) {
            if ((ja_task_state & JSUSPENDED) ||
                (ja_task_state & JSUSPENDED_ON_THRESHOLD)) {
#ifdef JOB_SPLIT_DEBUG
               DPRINTF("Task " sge_u32 " is in suspended state\n", ja_task_id);
#endif
               target = &(target_tasks[SPLIT_SUSPENDED]);
            } else {
               if ((lGetUlong(ja_task, JAT_state) & JSUSPENDED_ON_SUBORDINATE) ||
                   (lGetUlong(ja_task, JAT_state) & JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
#ifdef JOB_SPLIT_DEBUG
                  DPRINTF("Task " sge_u32 " is in suspended state\n",ja_task_id);
#endif
                  target = &(target_tasks[SPLIT_SUSPENDED]);
               }
            }
         }
         if (target == nullptr && result_list[SPLIT_RUNNING] &&
             ja_task_status != JIDLE) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is in running state\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_RUNNING]);
         } 
         if (target == nullptr && result_list[SPLIT_HOLD] &&
             (ja_task_hold & MINUS_H_TGT_ALL)) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is in hold state\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_HOLD]);
         } 
#ifdef JOB_SPLIT_DEBUG
         if (target == nullptr) {
            ERROR("Task " sge_u32 " has no known state: " "status=" sge_u32 " state=" sge_u32 "\n", ja_task_id, ja_task_status, ja_task_state);
         }
#endif

         /* 
          * Move the task into the target list
          */
         if (target != nullptr) {
            if (*target == nullptr) {
               *target = lCreateList(nullptr, lGetElemDescr(ja_task));
            }
            if (do_copy) {
               lAppendElem(*target, lCopyElem(ja_task));
            } else {
               lDechainElem(ja_task_list, ja_task);
               lAppendElem(*target, ja_task);
            }
         }
      }

      if (target_for_ids == SPLIT_LAST &&
          result_list[SPLIT_WAITING_DUE_TO_PREDECESSOR] &&
          lGetList(job, JB_jid_predecessor_list) != nullptr) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF("Unenrolled tasks are waiting for pred. jobs\n");
#endif
         target_for_ids = SPLIT_WAITING_DUE_TO_PREDECESSOR;
         target_ids = n_h_ids;
         n_h_ids = nullptr;
      }
      if (target_for_ids == SPLIT_LAST &&
          result_list[SPLIT_WAITING_DUE_TO_TIME] &&
          lGetUlong64(job, JB_execution_time) > sge_get_gmt64()) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF("Unenrolled tasks are waiting due to time\n");
#endif
         target_for_ids = SPLIT_WAITING_DUE_TO_TIME;
         target_ids = n_h_ids;
         n_h_ids = nullptr;
      }
      if (target_for_ids == SPLIT_LAST &&
          result_list[SPLIT_PENDING_EXCLUDED_INSTANCES] &&
          max_aj_instances > 0) {
         uint32_t task_concurrency = lGetUlong(job, JB_ja_task_concurrency);
         uint32_t max_aj_conc_instances = max_aj_instances;
         if (task_concurrency > 0 && task_concurrency < max_aj_instances) {
            max_aj_conc_instances = task_concurrency;
         }
         excluded_n_h_ids = n_h_ids;
         n_h_ids = nullptr;
         if (task_instances < max_aj_conc_instances) {
            uint32_t allowed_instances = max_aj_conc_instances - task_instances;
            range_list_move_first_n_ids(&excluded_n_h_ids, nullptr, &n_h_ids, allowed_instances);
         }
         target_for_ids = SPLIT_PENDING_EXCLUDED_INSTANCES;
         target_ids = excluded_n_h_ids;
      }

      /*
       * Copy/Move and insert job into the target lists
       */
      move_job = 1;
      for (i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
         if ((target_tasks[i] != nullptr) ||
             (i == target_for_ids && target_ids != nullptr) ||
             (i == SPLIT_PENDING && n_h_ids != nullptr ) ||
             (i == SPLIT_HOLD && (u_h_ids != nullptr || o_h_ids != nullptr ||
                                  s_h_ids != nullptr || a_h_ids != nullptr))) {
            if (result_list[i] != nullptr) {
               if (*(result_list[i]) == nullptr) {
                  const lDescr *reduced_decriptor = lGetElemDescr(job);

#ifdef JOB_SPLIT_DEBUG               
                  DPRINTF("Create " SFN "-list\n", get_name_of_split_value(i));
#endif
                  *(result_list[i]) = lCreateList("", reduced_decriptor);
               } 
               if (move_job == 1) {
#ifdef JOB_SPLIT_DEBUG
                  DPRINTF(("Reuse job element " sge_u32" for " SFN "-list\n",
                           lGetUlong(job, JB_job_number), get_name_of_split_value(i)));
#endif
                  move_job = 0;
                  if (do_copy) {
                     target_job[i] = lCopyElem(job);
                  } else {
                     lDechainElem(*job_list, job);
                     target_job[i] = job;
                  }
               } else {
#ifdef JOB_SPLIT_DEBUG
                  DPRINTF(("Copy job element " sge_u32 " for " SFN "-list\n",
                           lGetUlong(job, JB_job_number), get_name_of_split_value(i)));
#endif
                  target_job[i] = lCopyElem(job);
               }
#ifdef JOB_SPLIT_DEBUG
               DPRINTF(("Add job element " sge_u32 " into " SFN "-list\n",
                        lGetUlong(target_job[i], JB_job_number), get_name_of_split_value(i)));
#endif
               lAppendElem(*(result_list[i]), target_job[i]);
            }
         }
      }

      /*
       * Do we have remaining tasks which won't fit into the target lists?
       */
      if ((lGetNumberOfElem(ja_task_list) > 0) ||
          (result_list[SPLIT_PENDING] == nullptr && n_h_ids != nullptr) ||
          (result_list[SPLIT_HOLD] == nullptr &&
                   (u_h_ids != nullptr || o_h_ids != nullptr ||
                    s_h_ids != nullptr || a_h_ids == nullptr))) {
         if (move_job == 0 && !do_copy) {
            /* 
             * We moved 'job' into a target list therefore it is necessary 
             * to create a new job.
             */ 
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Put the remaining tasks into the initial container\n");
#endif
            job = lCopyElem(job);
            lInsertElem(*job_list, previous_job, job);
         }
      } else {
         job = nullptr;
      } 

      /* 
       * Insert array task information for not enrolled tasks
       */
      if (target_for_ids < SPLIT_LAST && result_list[target_for_ids] != nullptr && target_ids != nullptr) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF(("Move not enrolled %s tasks\n",
                  get_name_of_split_value(target_for_ids)));
#endif
         lXchgList(target_job[target_for_ids], JB_ja_n_h_ids, &target_ids);
      }
      if (result_list[SPLIT_PENDING] != nullptr && n_h_ids != nullptr) {
#ifdef JOB_SPLIT_DEBUG 
         DPRINTF("Move not enrolled pending tasks\n");
#endif
         lXchgList(target_job[SPLIT_PENDING], JB_ja_n_h_ids, &n_h_ids);
      }
      if (result_list[SPLIT_HOLD] != nullptr &&
          (u_h_ids != nullptr || o_h_ids != nullptr ||
           s_h_ids != nullptr || a_h_ids != nullptr)) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF("Move not enrolled hold tasks\n");
#endif
         lXchgList(target_job[SPLIT_HOLD], JB_ja_u_h_ids, &u_h_ids);
         lXchgList(target_job[SPLIT_HOLD], JB_ja_o_h_ids, &o_h_ids);
         lXchgList(target_job[SPLIT_HOLD], JB_ja_s_h_ids, &s_h_ids);
         lXchgList(target_job[SPLIT_HOLD], JB_ja_a_h_ids, &a_h_ids);
      }
      for (i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
         if (target_tasks[i] != nullptr) {
#ifdef JOB_SPLIT_DEBUG 
            DPRINTF("Put " SFQ "-tasks into job\n", get_name_of_split_value(i));
#endif
            lSetList(target_job[i], JB_ja_tasks, target_tasks[i]);
         }
      }
      
      /* 
       * Put remaining tasks into job 
       */
      if (job) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF("Put unenrolled tasks back into initial container\n");
#endif
         lXchgList(job, JB_ja_tasks, &ja_task_list);
         lXchgList(job, JB_ja_n_h_ids, &n_h_ids);
         lXchgList(job, JB_ja_u_h_ids, &u_h_ids);
         lXchgList(job, JB_ja_o_h_ids, &o_h_ids);
         lXchgList(job, JB_ja_s_h_ids, &s_h_ids);
         lXchgList(job, JB_ja_a_h_ids, &a_h_ids);
      } else {
         if (!do_copy) {
            lFreeList(&ja_task_list);
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Trash all not needed job lists
 *
 * Trash all job lists which are not needed for scheduling decisions.
 * Before jobs and lists are trashed, scheduling messages will
 * be generated.
 * Following lists will be trashed:
 *    splitted_job_lists[SPLIT_ERROR]
 *    splitted_job_lists[SPLIT_HOLD]
 *    splitted_job_lists[SPLIT_WAITING_DUE_TO_TIME]
 *    splitted_job_lists[SPLIT_WAITING_DUE_TO_PREDECESSOR]
 *    splitted_job_lists[SPLIT_PENDING_EXCLUDED_INSTANCES]
 *    splitted_job_lists[SPLIT_PENDING_EXCLUDED]
 *
 * @param[in]     monitor_next_run   whether the messages also go into the
 *                                   scheduler run log
 * @param[in,out] splitted_job_lists the array of job lists, indexed by the
 *                                   SPLIT_* values; the lists named above are
 *                                   freed
 *
 * @see #split_jobs, #job_lists_split_with_reference_to_max_running
 */
void trash_splitted_jobs(bool monitor_next_run, lList **splitted_job_lists[]) {
   int split_id_a[] = {
      SPLIT_ERROR, 
      SPLIT_HOLD, 
      SPLIT_WAITING_DUE_TO_TIME,
      SPLIT_WAITING_DUE_TO_PREDECESSOR,
      SPLIT_PENDING_EXCLUDED_INSTANCES,
      SPLIT_PENDING_EXCLUDED,
      SPLIT_LAST
   }; 
   int i = -1;

   while (split_id_a[++i] != SPLIT_LAST) { 
      lList **job_list = splitted_job_lists[split_id_a[i]];
      int is_first_of_category = 1;

      for_each_ep_lv(job, *job_list) {
         uint32_t job_id = lGetUlong(job, JB_job_number);

         switch (split_id_a[i]) {
         case SPLIT_ERROR:
            if (is_first_of_category) {
               /* for qstat -j schedd_messages */
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_JOBINERROR_);
            }
            /* for qalter -w v and qconf -tsm */
            schedd_log_list(nullptr, monitor_next_run,
                            MSG_LOG_JOBSDROPPEDERRORSTATEREACHED, 
                            *job_list, JB_job_number);
            break;
         case SPLIT_HOLD:
            if (is_first_of_category) {
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_JOBHOLD_);
            }
            schedd_log_list(nullptr, monitor_next_run,
                            MSG_LOG_JOBSDROPPEDBECAUSEOFXHOLD, 
                            *job_list, JB_job_number);
            break;
         case SPLIT_WAITING_DUE_TO_TIME:
            if (is_first_of_category) {
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_EXECTIME_);
            }
            schedd_log_list(nullptr, monitor_next_run,
                            MSG_LOG_JOBSDROPPEDEXECUTIONTIMENOTREACHED, 
                               *job_list, JB_job_number);
            break;
         case SPLIT_WAITING_DUE_TO_PREDECESSOR:
            if (is_first_of_category) {
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_JOBDEPEND_);
            }
            schedd_log_list(nullptr, monitor_next_run,
                            MSG_LOG_JOBSDROPPEDBECAUSEDEPENDENCIES, 
                               *job_list, JB_job_number);
            break;
         case SPLIT_PENDING_EXCLUDED_INSTANCES:
            if (is_first_of_category) {
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_MAX_AJ_INSTANCES_);
            }
            break;
         case SPLIT_PENDING_EXCLUDED:
            if (is_first_of_category) {
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_USRGRPLIMIT_);
            }
            break;
         default:
            ;
         }
         if (is_first_of_category) {
            is_first_of_category = 0;
            schedd_mes_commit(*job_list, 1, nullptr);
         } 
      }
      lFreeList(job_list);
   }
}

/**
 * @brief Writes the sizes of all split result lists to the debug output
 *
 * @param[in] job_list the array of result lists, indexed by the SPLIT_* values
 */
void job_lists_print(lList **job_list[]) {
   DENTER(TOP_LAYER);

   for (int i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
      uint32_t ids = 0;

      if (job_list[i] && *(job_list[i])) {
         for_each_ep_lv(job, *(job_list[i])) {
            ids += job_get_enrolled_ja_tasks(job);
            ids += job_get_not_enrolled_ja_tasks(job);
         }
         DPRINTF("job_list[%s] CONTAINES " sge_u32 " JOB(S) (" sge_u32 " TASK(S)\n",
                 get_name_of_split_value(i), lGetNumberOfElem(*(job_list[i])), ids);
      }
   } 

   DRETURN_VOID;
}

/**
 * @brief Lowers the job counter of one user
 *
 * The entry is removed once its count reaches zero, so the list only ever
 * holds users that actually have jobs.
 *
 * @param[in,out] jcpp  the job counter list (`JC_Type`)
 * @param[in]     name  the user the counter belongs to
 * @param[in]     slots how much to subtract
 */
void sge_dec_jc(lList **jcpp, const char *name, int slots) {
   DENTER(TOP_LAYER);

   int n = 0;
   lListElem *ep;

   ep = lGetElemStrRW(*jcpp, JC_name, name);
   if (ep) {
      n = lGetUlong(ep, JC_jobs) - slots;
      if (n <= 0)
         lDelElemStr(jcpp, JC_name, name);
      else
         lSetUlong(ep, JC_jobs, n);
   }

   DRETURN_VOID;
}

/**
 * @brief Raises the job counter of one user, creating the entry if needed
 *
 * @param[in,out] jcpp  the job counter list (`JC_Type`)
 * @param[in]     name  the user the counter belongs to
 * @param[in]     slots how much to add
 */
void sge_inc_jc(lList **jcpp, const char *name, int slots) {
   DENTER(TOP_LAYER);

   int n = 0;
   lListElem *ep;

   ep = lGetElemStrRW(*jcpp, JC_name, name);
   if (ep) 
      n = lGetUlong(ep, JC_jobs);
   else 
      ep = lAddElemStr(jcpp, JC_name, name, JC_Type);

   n += slots;

   lSetUlong(ep, JC_jobs, n);

   DRETURN_VOID;
}


/**
 * @brief Counts the slots granted to a job, optionally on one host
 *
 * @param[in] granted   the granted destination identifier list (`JG_Type`)
 * @param[in] qhostname the host to count on, or nullptr for all hosts
 *
 * @return the number of granted slots
 */
int nslots_granted(const lList *granted, const char *qhostname) {
   int nslots = 0;

   if (qhostname == nullptr) {
      for_each_ep_lv(gdil_ep, granted) {
         nslots += lGetUlong(gdil_ep, JG_slots);
      }
   } else {
      const void *iterator = nullptr;

      const lListElem *gdil_ep = lGetElemHostFirst(granted, JG_qhostname, qhostname, &iterator);
      while (gdil_ep != nullptr) {
         nslots += lGetUlong(gdil_ep, JG_slots);
         gdil_ep = lGetElemHostNext(granted, JG_qhostname , qhostname, &iterator);
      }
   }

   return nslots;
}

/**
 * @brief Does the job have active tasks in this queue?
 *
 * The master queue always counts as active, even when no task of the job is
 * currently running in it.
 *
 * @param[in] job   the job to look at
 * @param[in] qname the queue instance to look for
 *
 * @return 1 if the queue holds an active task of the job, 0 otherwise
 */
int active_subtasks(
lListElem *job,
const char *qname 
) {
   const lListElem *ep;
   const char *task_qname;

   for_each_ep_lv(jatask, lGetList(job, JB_ja_tasks)) {
      const char *master_qname = lGetString(jatask, JAT_master_queue);

      /* always consider the master queue to have active sub-tasks */
      if (master_qname && !strcmp(qname, master_qname)) {
         return 1;
      }

      for_each_ep_lv(petask, lGetList(jatask, JAT_task_list)) {
         if (qname &&
             lGetUlong(petask, PET_status) != JFINISHED &&
             ((ep=lFirst(lGetList(petask, PET_granted_destin_identifier_list)))) &&
             ((task_qname=lGetString(ep, JG_qname))) &&
             !strcmp(qname, task_qname)) {
            return 1;
         }   
      }
   }
   return 0;
}


/**
 * @brief Counts the granted slots that still carry active tasks
 *
 * Like nslots_granted(), but a host only contributes while the job has active
 * tasks there - see active_subtasks(). A job whose tasks have all finished on
 * a host no longer occupies its slots.
 *
 * @param[in] job       the job to look at
 * @param[in] granted   the granted destination identifier list (`JG_Type`)
 * @param[in] qhostname the host to count on, or nullptr for all hosts
 *
 * @return the number of granted slots that are still in use
 */
int 
active_nslots_granted(lListElem *job, const lList *granted, const char *qhostname) {
   int nslots = 0;

   if (qhostname == nullptr) {
      for_each_ep_lv(gdil_ep, granted) {   /* for all hosts */
         for_each_ep_lv(jatask, lGetList(job, JB_ja_tasks)) {
            const lList *task_list = lGetList(jatask, JAT_task_list);
            if (task_list == nullptr || active_subtasks(job, lGetString(gdil_ep, JG_qname)))
               nslots += lGetUlong(gdil_ep, JG_slots);
         }
      }
   } else {
      const void *iterator = nullptr;

      /* only for qhostname */
      lListElem *gdil_ep = lGetElemHostFirstRW(granted, JG_qhostname, qhostname, &iterator);
      while (gdil_ep != nullptr) {
         for_each_ep_lv(jatask, lGetList(job, JB_ja_tasks)) {
            const lList *task_list = lGetList(jatask, JAT_task_list);
            if (task_list == nullptr || active_subtasks(job, lGetString(gdil_ep, JG_qname)))
               nslots += lGetUlong(gdil_ep, JG_slots);
         }
         gdil_ep = lGetElemHostNextRW(granted, JG_qhostname , qhostname, &iterator); 
      }
   }

   return nslots;
}


/**
 * @brief Returns the total number of slots granted to a parallel job
 *
 * @param[in] gdil the granted destination identifier list (`JG_Type`)
 *
 * @return the sum of the slots of all entries
 */
int sge_granted_slots(const lList *gdil) {
   int slots = 0;

   for_each_ep_lv(ep, gdil)
      slots += lGetUlong(ep, JG_slots);

   return slots;
}
