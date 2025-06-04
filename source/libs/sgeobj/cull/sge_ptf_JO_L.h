#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/*
 * This code was generated from file source/libs/sgeobj/json/JO.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief OS Job
*
* This is the list type we use to hold the list of
* OS jobs being tracked for each PTF job. There
* will normally only be one OS job per PTF job,
* except in the case of tightly integrated parallel jobs or jobs with multiple tasks.
*
*    SGE_ULONG(JO_OS_job_ID) - OS Job Id
*    OS job id (lower 32 bits)
*
*    SGE_ULONG(JO_OS_job_ID2) - OS Job Id 2
*    OS job id (upper 32 bits). @todo Replace the two id fields by one u_long64?
*
*    SGE_STRING(JO_systemd_scope) - Systemd Scope
*    When the job is running in a systemd scope, this field contains the name of the scope.
*
*    SGE_ULONG(JO_ja_task_ID) - Array Task Id
*    In case of an array job: Task number of an array task.
*
*    SGE_STRING(JO_task_id_str) - Task Id String
*    In case of a tightly integrated parallel job: Pe Task Id String.
*
*    SGE_ULONG(JO_state) - State
*    Job state (JL_JOB_* values).
*
*    SGE_LIST(JO_usage_list) - Usage List
*    PTF interval usage values.
*
*    SGE_LIST(JO_pid_list) - Pid List
*    List of process Ids belonging to this job/task.
*
*/

enum {
   JO_OS_job_ID = JO_LOWERBOUND,
   JO_OS_job_ID2,
   JO_systemd_scope,
   JO_ja_task_ID,
   JO_task_id_str,
   JO_state,
   JO_usage_list,
   JO_pid_list
};

LISTDEF(JO_Type)
   SGE_ULONG(JO_OS_job_ID, CULL_DEFAULT)
   SGE_ULONG(JO_OS_job_ID2, CULL_DEFAULT)
   SGE_STRING(JO_systemd_scope, CULL_DEFAULT)
   SGE_ULONG(JO_ja_task_ID, CULL_DEFAULT)
   SGE_STRING(JO_task_id_str, CULL_DEFAULT)
   SGE_ULONG(JO_state, CULL_DEFAULT)
   SGE_LIST(JO_usage_list, UA_Type, CULL_DEFAULT)
   SGE_LIST(JO_pid_list, JP_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(JON)
   NAME("JO_OS_job_ID")
   NAME("JO_OS_job_ID2")
   NAME("JO_systemd_scope")
   NAME("JO_ja_task_ID")
   NAME("JO_task_id_str")
   NAME("JO_state")
   NAME("JO_usage_list")
   NAME("JO_pid_list")
NAMEEND

#define JO_SIZE sizeof(JON)/sizeof(char *)


