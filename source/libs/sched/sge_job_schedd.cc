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
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_JC.h"

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
 * @param jep The job
 *
 * @return true on success
 *
 * @note MT-NOTE: job_get_duration() is MT safe
 */
bool job_get_duration(uint64_t *duration, const ocs::tom::ConstElement<ocs::tom::JB> jep) {
   DENTER(TOP_LAYER);

   if (!job_get_wallclock_limit(duration, jep)) {
      *duration = sge_gmt32_to_gmt64(sconf_get_default_duration());
   }

   DRETURN(true);
}

/**
 * @brief job_get_duration() for an untyped caller
 *
 * @param duration Returns duration on success
 * @param jep The job (`JB_Type`), checked
 *
 * @return as job_get_duration()
 */
bool job_get_duration(uint64_t *duration, const lListElem *jep) {
   return job_get_duration(duration, ocs::tom::ConstElement<ocs::tom::JB>(jep));
}

/**
 * @brief Determin tasks effective runtime limit
 *
 * Determines the effictive runtime limit got by requested h_rt/s_rt or
 * by the resulting queues h_rt/s_rt
 *
 * @param duration tasks duration in seconds
 * @param ja_task task element, or an empty handle for the default duration
 *
 * @return true
 *
 * @note MT-NOTE: task_get_duration() is MT safe
 */
bool task_get_duration(uint64_t *duration, const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   DENTER(TOP_LAYER);

   if (ja_task != nullptr) {
      *duration = ocs::tom::get<JAT_wallclock_limit>(ja_task);
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
 *                               SPLIT_* values; the running list is created
 *                               when there is none yet
 *
 * @return true if the pending job was removed
 *
 * @see #split_jobs
 */
bool job_move_first_pending_to_running(lListElem **pending_job, ocs::tom::ListPtr<ocs::tom::JB> *splitted_jobs[]) {
   DENTER(TOP_LAYER);

   bool ret = false;
   const auto pending = ocs::tom::Element<ocs::tom::JB>(*pending_job);

   const uint32_t job_id = ocs::tom::get<JB_job_number>(pending);
   auto ja_task_list = ocs::tom::get<JB_ja_tasks>(pending);
   auto ja_task = ja_task_list.front();

   /*
    * Create list for running jobs
    */
   if (*(splitted_jobs[SPLIT_RUNNING]) == nullptr) {
      // the list belongs to the caller and gets the descriptor of the (possibly reduced) job
      *(splitted_jobs[SPLIT_RUNNING]) =
              ocs::tom::ListPtr<ocs::tom::JB>::from_typed(lCreateList("", lGetElemDescr(pending)));
   }
   const auto running_list = splitted_jobs[SPLIT_RUNNING]->get();
   auto running_job = running_list.find_first<JB_job_number>(job_id);

   /*
    * Create a running job if it does not exist aleady 
    */
   if (running_job == nullptr) {
      // copy the job without its tasks and task id lists, then give the lists back
      auto n_h_ids = pending.exchange<JB_ja_n_h_ids>(nullptr);
      auto u_h_ids = pending.exchange<JB_ja_u_h_ids>(nullptr);
      auto o_h_ids = pending.exchange<JB_ja_o_h_ids>(nullptr);
      auto s_h_ids = pending.exchange<JB_ja_s_h_ids>(nullptr);
      auto a_h_ids = pending.exchange<JB_ja_a_h_ids>(nullptr);
      auto r_tasks = pending.exchange<JB_ja_tasks>(nullptr);
      auto running_copy = pending.clone();
      pending.exchange<JB_ja_n_h_ids>(std::move(n_h_ids));
      pending.exchange<JB_ja_u_h_ids>(std::move(u_h_ids));
      pending.exchange<JB_ja_o_h_ids>(std::move(o_h_ids));
      pending.exchange<JB_ja_s_h_ids>(std::move(s_h_ids));
      pending.exchange<JB_ja_a_h_ids>(std::move(a_h_ids));
      pending.exchange<JB_ja_tasks>(std::move(r_tasks));
      running_job = running_list.push_back(std::move(running_copy));
   }

   /*
    * Create an array instance and add it to the running job
    * or move the existing array task into the running job
    */
   if (ja_task == nullptr) {
      const auto n_h_ids = ocs::tom::get<JB_ja_n_h_ids>(pending);

      const uint32_t ja_task_id = range_list_get_first_id(n_h_ids, nullptr);
      ja_task = ocs::tom::Element<ocs::tom::JAT>::from_typed(job_search_task(pending, nullptr, ja_task_id));
      /* JG: TODO: do we need the ja_task instance here or can we
       *           wait until the JATASK_ADD event arrives from qmaster?
       *           The function should work on a copy if the job list.
       *           The event from qmaster has effect on the mirrored lists.
       *           So the code should be ok.
       */
      if (ja_task == nullptr) {
         ja_task = ocs::tom::Element<ocs::tom::JAT>::from_typed(job_create_task(pending, nullptr, ja_task_id));
      }
      ja_task_list = ocs::tom::get<JB_ja_tasks>(pending);
   }

   /*
    * Create an array task list if necessary
    */
   auto r_ja_task_list = ocs::tom::get<JB_ja_tasks>(running_job);
   if (r_ja_task_list == nullptr) {
      // the list gets the descriptor of the (possibly reduced) task
      auto created = ocs::tom::ListPtr<ocs::tom::JAT>::from_typed(lCreateList("", lGetElemDescr(ja_task)));
      r_ja_task_list = created.get();
      ocs::tom::set<JB_ja_tasks>(running_job, std::move(created));
   }

   r_ja_task_list.push_back(ja_task_list.extract(ja_task));

   /*
    * Remove pending job if there are no pending tasks anymore
    */
   if (job_count_pending_tasks(pending, false) == 0) {
      splitted_jobs[SPLIT_PENDING]->get().erase(pending);
      *pending_job = nullptr;
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
 * @param[in] job the job to take the task from
 *
 * @return the task and its id, or std::nullopt if the job has no task left
 */
std::optional<std::pair<ocs::tom::Element<ocs::tom::JAT>, uint32_t>>
job_get_next_task(const ocs::tom::Element<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   uint32_t ja_task_id;

   auto ja_task = ocs::tom::get<JB_ja_tasks>(job).front();
   if (ja_task == nullptr) {
      // range_list_get_first_id() reports through a raw answer list
      lList *answer_list = nullptr;

      ja_task_id = range_list_get_first_id(ocs::tom::get<JB_ja_n_h_ids>(job), &answer_list);
      if (answer_list_has_error(&answer_list)) {
         lFreeList(&answer_list);
         DRETURN(std::nullopt);
      }
      ja_task = ocs::tom::Element<ocs::tom::JAT>::from_typed(job_get_ja_task_template_pending(job, ja_task_id));
   } else {
      ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
   }

   DRETURN(std::make_pair(ja_task, ja_task_id));
}

/**
 * @brief Inc. the # of jobs a user has running
 *
 * Initialize "user_list" and JC_jobs attribute for each user according
 * to the list of running jobs.
 *
 * @param[in,out] user_list           the job counters per user (`JC_Type`),
 *                                     created with the first counter
 * @param[in]     splitted_job_lists   the array of job lists, indexed by the
 *                                     SPLIT_* values; the running and the
 *                                     suspended list are counted
 */
void user_list_init_jc(ocs::tom::ListPtr<ocs::tom::JC> &user_list,
                       ocs::tom::ListPtr<ocs::tom::JB> *splitted_job_lists[]) {
   DENTER(TOP_LAYER);

   for (const int split : {SPLIT_RUNNING, SPLIT_SUSPENDED}) {
      if (splitted_job_lists[split] != nullptr) {
         for (const auto job : splitted_job_lists[split]->get()) {
            // @todo (CS-451) the 3rd argument to sge_inc_jc is "slots", but we pass the number of array tasks. Correct?
            sge_inc_jc(user_list, ocs::tom::get<JB_owner>(job), job_get_ja_tasks(job));
         }
      }
   }

   DRETURN_VOID;
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
 * @param[in]     user_list         the job counters per user (`JC_Type`)
 * @param[in]     user_name         the user to limit, or nullptr for all users
 * @param[in]     max_jobs_per_user the limit, the `max_u_jobs` of the
 *                                  scheduler configuration
 *
 * @note JC_jobs of the user elements contained in "user_list" has to be
 *       initialized properly before this function might be called.
 *
 * @see #trash_splitted_jobs, #split_jobs, #user_list_init_jc
 */
void job_lists_split_with_reference_to_max_running(const bool monitor_next_run,
                                                   ocs::tom::ListPtr<ocs::tom::JB> *job_lists[],
                                                   const ocs::tom::ConstList<ocs::tom::JC> user_list,
                                                   const char *user_name,
                                                   const uint32_t max_jobs_per_user) {
   DENTER(TOP_LAYER);
   if (max_jobs_per_user != 0 &&
       job_lists[SPLIT_PENDING] != nullptr &&
       *(job_lists[SPLIT_PENDING]) != nullptr &&
       job_lists[SPLIT_PENDING_EXCLUDED] != nullptr) {
      const auto pending_list = job_lists[SPLIT_PENDING]->get();
      ocs::tom::ConstElement<ocs::tom::JC> user;
      ocs::tom::ConstElement<ocs::tom::JC> next_user;
#ifndef CULL_NO_HASH
      /*
       * create a hash table on JB_owner to speedup
       * searching for jobs of a specific owner
       */
      cull_hash_new_check(pending_list, JB_owner, false);
#endif

      if (user_name == nullptr) {
         next_user = user_list.front();
      } else {
         next_user = user_list.find_first<JC_name>(user_name);
      }
      while ((user = next_user) != nullptr) {
         uint32_t jobs_for_user = ocs::tom::get<JC_jobs>(user);
         const char *jc_user_name = ocs::tom::get<JC_name>(user);

         if (user_name == nullptr) {
            next_user = user.next();
         } else {
            next_user = nullptr;
         }
         if (jobs_for_user >= max_jobs_per_user) {
            const void *user_iterator = nullptr;
            ocs::tom::Element<ocs::tom::JB> user_job;

            DPRINTF("USER %s reached limit of %d jobs\n", jc_user_name, max_jobs_per_user);
            // the next match is looked up before the current job leaves the list
            auto next_user_job = ocs::tom::find_first_rw<JB_owner>(pending_list, jc_user_name, &user_iterator);
            while ((user_job = next_user_job)) {
               next_user_job = ocs::tom::find_next_rw<JB_owner>(pending_list, jc_user_name, &user_iterator);
               schedd_mes_add(nullptr, monitor_next_run,
                              ocs::tom::get<JB_job_number>(user_job),
                              SCHEDD_INFO_USRGRPLIMIT_);

               auto excluded_job = pending_list.extract(user_job);
               if (*(job_lists[SPLIT_PENDING_EXCLUDED]) == nullptr) {
                  // the list belongs to the caller and gets the descriptor of the (possibly reduced) job
                  *(job_lists[SPLIT_PENDING_EXCLUDED]) =
                          ocs::tom::ListPtr<ocs::tom::JB>::from_typed(lCreateList("", lGetElemDescr(user_job)));
               }

               (*(job_lists[SPLIT_PENDING_EXCLUDED]))->push_back(std::move(excluded_job));
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
 *                                 jobs are moved out of it unless `do_copy`.
 *                                 The list itself belongs to the caller and
 *                                 is never replaced or freed, also when it is
 *                                 left empty; with `do_copy` its jobs are
 *                                 taken apart and put together again
 * @param[in]     max_aj_instances maximum number of tasks of an array job
 *                                 that may be instantiated at the same time
 * @param[out]    result_list      the array of result lists (`JB_Type`),
 *                                 indexed by the SPLIT_* values; a nullptr
 *                                 entry is not collected, a result list is
 *                                 created with its first job
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
void split_jobs(const ocs::tom::List<ocs::tom::JB> job_list, const uint32_t max_aj_instances,
                ocs::tom::ListPtr<ocs::tom::JB> *result_list[], const bool do_copy) {
   DENTER(TOP_LAYER);

#if 0 /* EB: DEBUG: enable debug messages for split_jobs() */
#define JOB_SPLIT_DEBUG
#endif
   // the input list belongs to the caller; the handle does not replace it
   const auto jobs = job_list;
   ocs::tom::Element<ocs::tom::JB> job;
   auto next_job = jobs.front();
   ocs::tom::Element<ocs::tom::JB> previous_job;
   while ((job = next_job)) {
      // the tasks of each split target, and the job element each target list gets
      ocs::tom::ListPtr<ocs::tom::JAT> target_tasks[SPLIT_LAST];
      ocs::tom::Element<ocs::tom::JB> target_job[SPLIT_LAST];
      ocs::tom::ListPtr<ocs::tom::RN> target_ids;
      int target_for_ids = SPLIT_LAST;
      ocs::tom::Element<ocs::tom::JAT> ja_task;
      ocs::tom::Element<ocs::tom::JAT> next_ja_task;
      int i, move_job;
#ifdef JOB_SPLIT_DEBUG
      uint32_t job_id = ocs::tom::get<JB_job_number>(job);
#endif

      previous_job = job.prev();
      next_job = job.next();

      const auto task_instances = static_cast<uint32_t>(ocs::tom::get<JB_ja_tasks>(job).size());

      /*
       * Remove all ballast for a minimal copy operation of the job
       */
      auto ja_task_list = job.exchange<JB_ja_tasks>(nullptr);
      auto n_h_ids = job.exchange<JB_ja_n_h_ids>(nullptr);
      auto u_h_ids = job.exchange<JB_ja_u_h_ids>(nullptr);
      auto o_h_ids = job.exchange<JB_ja_o_h_ids>(nullptr);
      auto s_h_ids = job.exchange<JB_ja_s_h_ids>(nullptr);
      auto a_h_ids = job.exchange<JB_ja_a_h_ids>(nullptr);

      /*
       * Split enrolled tasks
       */
#ifdef JOB_SPLIT_DEBUG
      DPRINTF("Split enrolled tasks for job " sge_u32 ":\n", job_id);
#endif
      next_ja_task = ja_task_list->front();
      while ((ja_task = next_ja_task)) {
         uint32_t ja_task_status = ocs::tom::get<JAT_status>(ja_task);
         uint32_t ja_task_state = ocs::tom::get<JAT_state>(ja_task);
         uint32_t ja_task_hold = ocs::tom::get<JAT_hold>(ja_task);
         ocs::tom::ListPtr<ocs::tom::JAT> *target = nullptr;
#ifdef JOB_SPLIT_DEBUG
         uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
#endif
         next_ja_task = ja_task.next();

#ifdef JOB_SPLIT_DEBUG
         DPRINTF(("Task " sge_u32 ": status=" sge_u32 " state=" sge_u32"\n", ja_task_id, ja_task_status, ja_task_state));
#endif

         /*
          * Check the state of the task
          * (ORDER IS IMPORTANT!)
          */
         // sge_qmaster sets JDEFERRED_REQ in JAT_state after qmod -r until the old run is gone,
         // only a pending task waits for it, e.g. a task deleted meanwhile is finished
         if (target == nullptr && result_list[SPLIT_DEFERRED] &&
             (ja_task_state & JDEFERRED_REQ) && ja_task_status == JIDLE) {
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
             (ocs::tom::get<JB_execution_time>(job) > sge_get_gmt64()) &&
             (ja_task_status == JIDLE)) {
#ifdef JOB_SPLIT_DEBUG
            DPRINTF("Task " sge_u32 " is waiting due to time.\n", ja_task_id);
#endif
            target = &(target_tasks[SPLIT_WAITING_DUE_TO_TIME]);
         }
         if (target == nullptr && result_list[SPLIT_WAITING_DUE_TO_PREDECESSOR] &&
             (ocs::tom::get<JB_jid_predecessor_list>(job) != nullptr) &&
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
               if ((ocs::tom::get<JAT_state>(ja_task) & JSUSPENDED_ON_SUBORDINATE) ||
                   (ocs::tom::get<JAT_state>(ja_task) & JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
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
               // the list gets the descriptor of the (possibly reduced) task
               *target = ocs::tom::ListPtr<ocs::tom::JAT>::from_typed(lCreateList(nullptr, lGetElemDescr(ja_task)));
            }
            if (do_copy) {
               (*target)->push_back(ja_task.clone());
            } else {
               (*target)->push_back(ja_task_list->extract(ja_task));
            }
         }
      }

      if (target_for_ids == SPLIT_LAST &&
          result_list[SPLIT_WAITING_DUE_TO_PREDECESSOR] &&
          ocs::tom::get<JB_jid_predecessor_list>(job) != nullptr) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF("Unenrolled tasks are waiting for pred. jobs\n");
#endif
         target_for_ids = SPLIT_WAITING_DUE_TO_PREDECESSOR;
         target_ids = std::move(n_h_ids);
      }
      if (target_for_ids == SPLIT_LAST &&
          result_list[SPLIT_WAITING_DUE_TO_TIME] &&
          ocs::tom::get<JB_execution_time>(job) > sge_get_gmt64()) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF("Unenrolled tasks are waiting due to time\n");
#endif
         target_for_ids = SPLIT_WAITING_DUE_TO_TIME;
         target_ids = std::move(n_h_ids);
      }
      if (target_for_ids == SPLIT_LAST &&
          result_list[SPLIT_PENDING_EXCLUDED_INSTANCES] &&
          max_aj_instances > 0) {
         uint32_t task_concurrency = ocs::tom::get<JB_ja_task_concurrency>(job);
         uint32_t max_aj_conc_instances = max_aj_instances;
         if (task_concurrency > 0 && task_concurrency < max_aj_instances) {
            max_aj_conc_instances = task_concurrency;
         }
         // all not enrolled ids are excluded, except the first ones that still fit
         target_ids = std::move(n_h_ids);
         if (task_instances < max_aj_conc_instances) {
            uint32_t allowed_instances = max_aj_conc_instances - task_instances;
            // range_list_move_first_n_ids() works on raw lists, it may create the one and free the other
            lList *excluded = target_ids.release();
            lList *allowed = nullptr;
            range_list_move_first_n_ids(&excluded, nullptr, &allowed, allowed_instances);
            target_ids = ocs::tom::ListPtr<ocs::tom::RN>::from_typed(excluded);
            n_h_ids = ocs::tom::ListPtr<ocs::tom::RN>::from_typed(allowed);
         }
         target_for_ids = SPLIT_PENDING_EXCLUDED_INSTANCES;
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
               ocs::tom::ElementPtr<ocs::tom::JB> new_job;

               if (*(result_list[i]) == nullptr) {
#ifdef JOB_SPLIT_DEBUG               
                  DPRINTF("Create " SFN "-list\n", get_name_of_split_value(i));
#endif
                  // the list belongs to the caller and gets the descriptor of the (possibly reduced) job
                  *(result_list[i]) = ocs::tom::ListPtr<ocs::tom::JB>::from_typed(lCreateList("", lGetElemDescr(job)));
               } 
               if (move_job == 1) {
#ifdef JOB_SPLIT_DEBUG
                  DPRINTF(("Reuse job element " sge_u32" for " SFN "-list\n",
                           ocs::tom::get<JB_job_number>(job), get_name_of_split_value(i)));
#endif
                  move_job = 0;
                  if (do_copy) {
                     new_job = job.clone();
                  } else {
                     new_job = jobs.extract(job);
                  }
               } else {
#ifdef JOB_SPLIT_DEBUG
                  DPRINTF(("Copy job element " sge_u32 " for " SFN "-list\n",
                           ocs::tom::get<JB_job_number>(job), get_name_of_split_value(i)));
#endif
                  new_job = job.clone();
               }
#ifdef JOB_SPLIT_DEBUG
               DPRINTF(("Add job element " sge_u32 " into " SFN "-list\n",
                        ocs::tom::get<JB_job_number>(new_job.get()), get_name_of_split_value(i)));
#endif
               target_job[i] = (*(result_list[i]))->push_back(std::move(new_job));
            }
         }
      }

      /*
       * Do we have remaining tasks which won't fit into the target lists?
       */
      if (!ja_task_list->empty() ||
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
            auto remaining_job = job.clone();
            if (previous_job != nullptr) {
               job = jobs.insert_after(previous_job, std::move(remaining_job));
            } else {
               job = jobs.push_front(std::move(remaining_job));
            }
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
         target_ids = target_job[target_for_ids].exchange<JB_ja_n_h_ids>(std::move(target_ids));
      }
      if (result_list[SPLIT_PENDING] != nullptr && n_h_ids != nullptr) {
#ifdef JOB_SPLIT_DEBUG 
         DPRINTF("Move not enrolled pending tasks\n");
#endif
         n_h_ids = target_job[SPLIT_PENDING].exchange<JB_ja_n_h_ids>(std::move(n_h_ids));
      }
      if (result_list[SPLIT_HOLD] != nullptr &&
          (u_h_ids != nullptr || o_h_ids != nullptr ||
           s_h_ids != nullptr || a_h_ids != nullptr)) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF("Move not enrolled hold tasks\n");
#endif
         u_h_ids = target_job[SPLIT_HOLD].exchange<JB_ja_u_h_ids>(std::move(u_h_ids));
         o_h_ids = target_job[SPLIT_HOLD].exchange<JB_ja_o_h_ids>(std::move(o_h_ids));
         s_h_ids = target_job[SPLIT_HOLD].exchange<JB_ja_s_h_ids>(std::move(s_h_ids));
         a_h_ids = target_job[SPLIT_HOLD].exchange<JB_ja_a_h_ids>(std::move(a_h_ids));
      }
      for (i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
         if (target_tasks[i] != nullptr) {
#ifdef JOB_SPLIT_DEBUG 
            DPRINTF("Put " SFQ "-tasks into job\n", get_name_of_split_value(i));
#endif
            ocs::tom::set<JB_ja_tasks>(target_job[i], std::move(target_tasks[i]));
         }
      }
      
      /* 
       * Put remaining tasks into job 
       */
      if (job) {
#ifdef JOB_SPLIT_DEBUG
         DPRINTF("Put unenrolled tasks back into initial container\n");
#endif
         ja_task_list = job.exchange<JB_ja_tasks>(std::move(ja_task_list));
         n_h_ids = job.exchange<JB_ja_n_h_ids>(std::move(n_h_ids));
         u_h_ids = job.exchange<JB_ja_u_h_ids>(std::move(u_h_ids));
         o_h_ids = job.exchange<JB_ja_o_h_ids>(std::move(o_h_ids));
         s_h_ids = job.exchange<JB_ja_s_h_ids>(std::move(s_h_ids));
         a_h_ids = job.exchange<JB_ja_a_h_ids>(std::move(a_h_ids));
      }
      // whatever is left in the local lists is freed with them
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
void trash_splitted_jobs(const bool monitor_next_run, ocs::tom::ListPtr<ocs::tom::JB> *splitted_job_lists[]) {
   DENTER(TOP_LAYER);

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
      ocs::tom::ListPtr<ocs::tom::JB> *job_list = splitted_job_lists[split_id_a[i]];
      const auto jobs = job_list->get();
      int is_first_of_category = 1;

      for (const auto job : jobs) {
         const uint32_t job_id = ocs::tom::get<JB_job_number>(job);

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
                            jobs, JB_job_number);
            break;
         case SPLIT_HOLD:
            if (is_first_of_category) {
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_JOBHOLD_);
            }
            schedd_log_list(nullptr, monitor_next_run,
                            MSG_LOG_JOBSDROPPEDBECAUSEOFXHOLD, 
                            jobs, JB_job_number);
            break;
         case SPLIT_WAITING_DUE_TO_TIME:
            if (is_first_of_category) {
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_EXECTIME_);
            }
            schedd_log_list(nullptr, monitor_next_run,
                            MSG_LOG_JOBSDROPPEDEXECUTIONTIMENOTREACHED, 
                               jobs, JB_job_number);
            break;
         case SPLIT_WAITING_DUE_TO_PREDECESSOR:
            if (is_first_of_category) {
               schedd_mes_add(nullptr, monitor_next_run, job_id,
                              SCHEDD_INFO_JOBDEPEND_);
            }
            schedd_log_list(nullptr, monitor_next_run,
                            MSG_LOG_JOBSDROPPEDBECAUSEDEPENDENCIES, 
                               jobs, JB_job_number);
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
            schedd_mes_commit(jobs, 1, ocs::tom::Element<ocs::tom::CT>());
         } 
      }
      // the lists belong to the caller, which gets them back freed
      job_list->reset();
   }

   DRETURN_VOID;
}

/**
 * @brief Writes the sizes of all split result lists to the debug output
 *
 * @param[in] job_list the array of result lists, indexed by the SPLIT_* values
 */
void job_lists_print(ocs::tom::ListPtr<ocs::tom::JB> *job_list[]) {
   DENTER(TOP_LAYER);

   for (int i = SPLIT_FIRST; i < SPLIT_LAST; i++) {
      uint32_t ids = 0;

      if (job_list[i] && *(job_list[i])) {
         const auto jobs = ocs::tom::ConstList<ocs::tom::JB>(job_list[i]->get());
         for (const auto job : jobs) {
            ids += job_get_enrolled_ja_tasks(job);
            ids += job_get_not_enrolled_ja_tasks(job);
         }
         DPRINTF("job_list[%s] CONTAINES " sge_u32 " JOB(S) (" sge_u32 " TASK(S)\n",
                 get_name_of_split_value(i), static_cast<uint32_t>(jobs.size()), ids);
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

   auto ep = ocs::tom::find_first_rw<JC_name>(*jcpp, name);
   if (ep) {
      n = ocs::tom::get<JC_jobs>(ep) - slots;
      if (n <= 0)
         ocs::tom::erase<JC_name>(jcpp, name);
      else
         ocs::tom::set<JC_jobs>(ep, n);
   }

   DRETURN_VOID;
}

/**
 * @brief Raises the job counter of one user, creating the entry if needed
 *
 * @param[in,out] jc_list the job counter list (`JC_Type`), created with its first entry
 * @param[in]     name    the user the counter belongs to
 * @param[in]     slots   how much to add
 */
void sge_inc_jc(ocs::tom::ListPtr<ocs::tom::JC> &jc_list, const char *name, const int slots) {
   DENTER(TOP_LAYER);

   int n = 0;

   auto ep = jc_list->find_first<JC_name>(name);
   if (ep) {
      n = ocs::tom::get<JC_jobs>(ep);
   } else {
      // created as the free emplace_back() created it: unnamed and hashed
      if (jc_list == nullptr) {
         jc_list = ocs::tom::ListPtr<ocs::tom::JC>::create("");
      }
      ep = jc_list->emplace_back<JC_name>(name);
   }

   n += slots;

   ocs::tom::set<JC_jobs>(ep, n);

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
int nslots_granted(const ocs::tom::ConstList<ocs::tom::JG> granted, const char *qhostname) {
   int nslots = 0;

   if (qhostname == nullptr) {
      for (const auto gdil_ep : granted) {
         nslots += ocs::tom::get<JG_slots>(gdil_ep);
      }
   } else {
      const void *iterator = nullptr;

      auto gdil_ep = ocs::tom::find_first<JG_qhostname>(granted, qhostname, &iterator);
      while (gdil_ep != nullptr) {
         nslots += ocs::tom::get<JG_slots>(gdil_ep);
         gdil_ep = ocs::tom::find_next<JG_qhostname>(granted , qhostname, &iterator);
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
int active_subtasks(const ocs::tom::ConstElement<ocs::tom::JB> job, const char *qname) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::JG> ep;
   const char *task_qname;

   for (const auto jatask : ocs::tom::get<JB_ja_tasks>(job)) {
      const char *master_qname = ocs::tom::get<JAT_master_queue>(jatask);

      /* always consider the master queue to have active sub-tasks */
      if (master_qname && !strcmp(qname, master_qname)) {
         DRETURN(1);
      }

      for (const auto petask : ocs::tom::get<JAT_task_list>(jatask)) {
         if (qname &&
             ocs::tom::get<PET_status>(petask) != JFINISHED &&
             ((ep = ocs::tom::get<PET_granted_destin_identifier_list>(petask).front())) &&
             ((task_qname=ocs::tom::get<JG_qname>(ep))) &&
             !strcmp(qname, task_qname)) {
            DRETURN(1);
         }   
      }
   }
   DRETURN(0);
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
active_nslots_granted(const ocs::tom::ConstElement<ocs::tom::JB> job, const ocs::tom::ConstList<ocs::tom::JG> granted, const char *qhostname) {
   int nslots = 0;

   if (qhostname == nullptr) {
      for (const auto gdil_ep : granted) {   /* for all hosts */
         for (const auto jatask : ocs::tom::get<JB_ja_tasks>(job)) {
            const auto task_list = ocs::tom::get<JAT_task_list>(jatask);
            if (task_list == nullptr || active_subtasks(job, ocs::tom::get<JG_qname>(gdil_ep)))
               nslots += ocs::tom::get<JG_slots>(gdil_ep);
         }
      }
   } else {
      const void *iterator = nullptr;

      /* only for qhostname */
      auto gdil_ep = ocs::tom::find_first<JG_qhostname>(granted, qhostname, &iterator);
      while (gdil_ep != nullptr) {
         for (const auto jatask : ocs::tom::get<JB_ja_tasks>(job)) {
            const auto task_list = ocs::tom::get<JAT_task_list>(jatask);
            if (task_list == nullptr || active_subtasks(job, ocs::tom::get<JG_qname>(gdil_ep)))
               nslots += ocs::tom::get<JG_slots>(gdil_ep);
         }
         gdil_ep = ocs::tom::find_next<JG_qhostname>(granted, qhostname, &iterator);
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
int sge_granted_slots(const ocs::tom::ConstList<ocs::tom::JG> gdil) {
   int slots = 0;

   for (const auto ep : gdil) {
      slots += ocs::tom::get<JG_slots>(ep);
   }

   return slots;
}
