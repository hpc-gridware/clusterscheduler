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
 * This code was generated from file source/libs/sgeobj/json/PETR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief PE Task Request
*
* Objects of PETR_Type are used to request the start of a task in a
* tightly integrated parallel job.
*
*    SGE_ULONG(PETR_jobid) - Job ID
*    Job id of the job to start the task in.
*
*    SGE_ULONG(PETR_jataskid) - Array Task ID
*    Array task id of the ja_task to start the pe task in.
*
*    SGE_STRING(PETR_queuename) - Queue Name
*    Optionally: Name of the queue to start the job in.
*
*    SGE_STRING(PETR_owner) - Owner
*    Owner of the pe task.
*
*    SGE_STRING(PETR_cwd) - Current Working Directory
*    Current working directory for the execution of the task.
*
*    SGE_LIST(PETR_path_aliases) - Path Aliases
*    Optional: Path alias configuration.
*
*    SGE_LIST(PETR_environment) - Environment
*    Environment variables to set / to overwrite the job environment with (from qrsh -inherit -v).
*
*    SGE_ULONG64(PETR_submission_time) - Submission Time
*    Time when qrsh -inherit was started in microseconds since epoch.
*
*/

enum {
   PETR_jobid = PETR_LOWERBOUND,
   PETR_jataskid,
   PETR_queuename,
   PETR_owner,
   PETR_cwd,
   PETR_path_aliases,
   PETR_environment,
   PETR_submission_time
};

LISTDEF(PETR_Type)
   SGE_ULONG(PETR_jobid, CULL_DEFAULT)
   SGE_ULONG(PETR_jataskid, CULL_DEFAULT)
   SGE_STRING(PETR_queuename, CULL_DEFAULT)
   SGE_STRING(PETR_owner, CULL_DEFAULT)
   SGE_STRING(PETR_cwd, CULL_DEFAULT)
   SGE_LIST(PETR_path_aliases, PA_Type, CULL_DEFAULT)
   SGE_LIST(PETR_environment, VA_Type, CULL_DEFAULT)
   SGE_ULONG64(PETR_submission_time, CULL_DEFAULT)
LISTEND

NAMEDEF(PETRN)
   NAME("PETR_jobid")
   NAME("PETR_jataskid")
   NAME("PETR_queuename")
   NAME("PETR_owner")
   NAME("PETR_cwd")
   NAME("PETR_path_aliases")
   NAME("PETR_environment")
   NAME("PETR_submission_time")
NAMEEND

#define PETR_SIZE sizeof(PETRN)/sizeof(char *)


