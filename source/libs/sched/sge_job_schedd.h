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
#include <optional>
#include <utility>
#include "cull/cull.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JC.h"
#include "sgeobj/tom/ocs_tom_JG.h"

bool job_get_duration(uint64_t *duration, ocs::tom::ConstElement<ocs::tom::JB> jep);
OCS_TRANSITIONAL bool job_get_duration(uint64_t *duration, const lListElem *jep);
bool task_get_duration(uint64_t *duration, ocs::tom::ConstElement<ocs::tom::JAT> ja_task);

void sge_inc_jc(ocs::tom::ListPtr<ocs::tom::JC> &jc_list, const char *name, int slots);

void sge_dec_jc(lList** jcpp, const char *name, int slots);

std::optional<std::pair<ocs::tom::Element<ocs::tom::JAT>, uint32_t>>
job_get_next_task(ocs::tom::Element<ocs::tom::JB> job);

int nslots_granted(ocs::tom::ConstList<ocs::tom::JG> granted, const char *qhostname);

int active_subtasks(ocs::tom::ConstElement<ocs::tom::JB> job, const char *qname);

int active_nslots_granted(ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstList<ocs::tom::JG> granted,
                          const char *qhostname);

//lListElem *explicit_job_request(lListElem *jep, const char *name);

int sge_granted_slots(ocs::tom::ConstList<ocs::tom::JG> gdil);

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
split_jobs(ocs::tom::List<ocs::tom::JB> job_list, uint32_t max_aj_instances,
           ocs::tom::ListPtr<ocs::tom::JB> *result_lists[], bool do_copy);

void
job_lists_split_with_reference_to_max_running(bool monitor_next_run, ocs::tom::ListPtr<ocs::tom::JB> *job_lists[],
                                              ocs::tom::ConstList<ocs::tom::JC> user_list,
                                              const char *user_name,
                                              uint32_t max_jobs_per_user);

bool
job_move_first_pending_to_running(lListElem **pending_job,
                                  ocs::tom::ListPtr<ocs::tom::JB> *result_lists[]);

void
trash_splitted_jobs(bool monitor_next_run, ocs::tom::ListPtr<ocs::tom::JB> *job_list[]);

void
job_lists_print(ocs::tom::ListPtr<ocs::tom::JB> *job_list[]);

void
user_list_init_jc(ocs::tom::ListPtr<ocs::tom::JC> &user_list, ocs::tom::ListPtr<ocs::tom::JB> *splitted_job_lists[]);
