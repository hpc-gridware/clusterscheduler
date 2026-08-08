#pragma once
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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
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

#include <cinttypes>
#include "cull/cull.h"

bool job_get_duration(uint64_t *duration, const lListElem *jep);
bool task_get_duration(uint64_t *duration, const lListElem *ja_task);

void sge_inc_jc(lList** jcpp, const char *name, int slots);

void sge_dec_jc(lList** jcpp, const char *name, int slots);

int job_get_next_task(lListElem *job, lListElem **task_ret, uint32_t *id_ret);

/**
 * @name Dead declarations
 *
 * These ten functions were the predecessor of split_jobs(): one call per
 * reason, each moving the matching jobs from `jobs` into its own list. They
 * are **declared here and defined nowhere**, and nothing in the source tree
 * calls them - the only remaining trace is a comment in
 * `daemons/qmaster/sge_sched_thread.cc` that still mentions
 * sge_move_to_running(). split_jobs() does the whole split in one pass and
 * puts the results into an array indexed by the SPLIT_* values.
 *
 * @warning Do not call any of them - linking would fail.
 * @{
 */
/**
 * @brief Drops all running jobs into the running list
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param jobs         the job list to take from
 * @param running      receives the running jobs
 * @param running_name name given to the created list
 *
 * @return the number of jobs moved
 */
int sge_split_job_running(lList **jobs, lList **running, const char *running_name);

/**
 * @brief Moves the first task of a job to the running list
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param jobs    the job list the job belongs to
 * @param running receives the task
 * @param job     the job whose first task is moved
 *
 * @return the number of tasks moved
 */
int sge_move_to_running(lList **jobs, lList **running, lListElem *job);

/**
 * @brief Drops all finished jobs into the finished list
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param jobs          the job list to take from
 * @param finished      receives the finished jobs
 * @param finished_name name given to the created list
 *
 * @return the number of jobs moved
 */
int sge_split_job_finished(lList **jobs, lList **finished, const char *finished_name);

/**
 * @brief Drops all jobs whose start time has not been reached into the waiting list
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param jobs         the job list to take from
 * @param waiting      receives the waiting jobs
 * @param waiting_name name given to the created list
 * @param now          the current time the start times are compared against
 *
 * @return the number of jobs moved
 */
int sge_split_job_wait_at_time(lList **jobs, lList **waiting, const char *waiting_name, uint32_t now);

/**
 * @brief Drops all jobs in error state into the error list
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param jobs       the job list to take from
 * @param error      receives the jobs in error state
 * @param error_name name given to the created list
 *
 * @return the number of jobs moved
 */
int sge_split_job_error(lList **jobs, lList **error, const char *error_name);

/**
 * @brief Drops all jobs in hold state into the hold list
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param jobs      the job list to take from
 * @param hold      receives the jobs in hold
 * @param hold_name name given to the created list
 *
 * @return the number of jobs moved
 */
int sge_split_job_hold(lList **jobs, lList **hold, const char *hold_name);

/**
 * @brief Drops all jobs waiting for a predecessor into the waiting list
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param jobs         the job list to take from
 * @param waiting      receives the waiting jobs
 * @param waiting_name name given to the created list
 *
 * @return the number of jobs moved
 */
int sge_split_job_wait_predecessor(lList **jobs, lList **waiting, const char *waiting_name);

/**
 * @brief Drops all jobs restricted by a checkpointing environment
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param jobs            the job list to take from
 * @param restricted      receives the restricted jobs
 * @param restricted_name name given to the created list
 * @param ckpt_list       the checkpointing environments to check against
 *
 * @return the number of jobs moved
 */
int sge_split_job_ckpt_restricted(lList **jobs, lList **restricted, const char *restricted_name, lList *ckpt_list);

/**
 * @brief Filters the pending jobs against the per user job limit, one step
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param pending_jobs the pending jobs to filter
 * @param running_jobs the running jobs, counted against the limit
 * @param jct_list     the per user job counters, updated in place
 * @param max_jobs     the limit per user
 * @param elem         the field the jobs are counted by, e.g. the owner
 *
 * @return the jobs that passed the filter
 */
lList *filter_max_running_1step(lList *pending_jobs, lList *running_jobs, lList **jct_list, int max_jobs, int elem);

/**
 * @brief Filters the pending jobs against the per user job limit
 *
 * Dead - declared here, defined nowhere. See the group description above.
 *
 * @param pending_jobs the pending jobs to filter
 * @param jct_list     the per user job counters
 * @param max_jobs     the limit per user
 * @param elem         the field the jobs are counted by, e.g. the owner
 *
 * @return the jobs that passed the filter
 */
lList *filter_max_running(lList *pending_jobs, lList *jct_list, int max_jobs, int elem);
/** @} */

int nslots_granted(const lList *granted, const char *qhostname);

int active_subtasks(lListElem *job, const char *qname);

int active_nslots_granted(lListElem *job, const lList *granted, const char *qhostname);

//lListElem *explicit_job_request(lListElem *jep, const char *name);

int sge_granted_slots(const lList *gdil);

const char *get_name_of_split_value(int value);

/**
 * @brief Constants used for split_jobs()
 *
 * The values index the array of result lists split_jobs() fills, so every one
 * of them names one reason a job is or is not a candidate of this scheduling
 * run. #SPLIT_FIRST and #SPLIT_LAST are the loop bounds.
 *
 * @see #split_jobs, #trash_splitted_jobs
 */
enum {
   SPLIT_FIRST,                        ///< First value, for loops over the result lists

   SPLIT_PENDING = SPLIT_FIRST,        ///< Pending jobs/tasks which may be dispatched
   SPLIT_PENDING_EXCLUDED,             ///< Pending jobs/tasks that would exceed `max_u_jobs`
   SPLIT_PENDING_EXCLUDED_INSTANCES,   ///< Pending jobs/tasks that would exceed `max_aj_instances`
   SPLIT_SUSPENDED,                    ///< Suspended jobs/tasks
   SPLIT_WAITING_DUE_TO_PREDECESSOR,   ///< Jobs/tasks waiting for another job to finish
   SPLIT_HOLD,                         ///< Jobs/tasks in user, operator or system hold
   SPLIT_ERROR,                        ///< Jobs/tasks in error state
   SPLIT_WAITING_DUE_TO_TIME,          ///< Jobs/tasks whose start time is in the future
   SPLIT_RUNNING,                      ///< Jobs/tasks that are already running
   SPLIT_FINISHED,                     ///< Jobs/tasks that have finished
   SPLIT_NOT_STARTED,                  ///< Jobs that could not be dispatched in this run
   SPLIT_DEFERRED,                     ///< Jobs held back for a later scheduling run
   SPLIT_LAST                          ///< Number of result lists, for loops
};

void 
split_jobs(lList **job_list, uint32_t max_aj_instances,
           lList **result_lists[], bool do_copy); 

void 
job_lists_split_with_reference_to_max_running(bool monitor_next_run, lList **job_lists[],
                                              lList **user_list,
                                              const char *user_name,
                                              uint32_t max_jobs_per_user);

bool
job_move_first_pending_to_running(lListElem **pending_job,
                                  lList **result_lists[]);

void 
trash_splitted_jobs(bool monitor_next_run, lList **job_list[]);

void 
job_lists_print(lList **job_list[]);

void 
user_list_init_jc(lList **user_list, lList **splitted_job_lists[]);
