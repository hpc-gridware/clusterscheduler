#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/JJ.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief JAPI Job
*
* Element holding per job information about JAPI session.
*
*    SGE_ULONG(JJ_jobid) - Job Id
*    JAPI job id.
*
*    SGE_ULONG(JJ_type) - Type
*    Job type - analoguous to JB_type.
*
*    SGE_LIST(JJ_finished_tasks) - Finished Tasks
*    List of finished job tasks.
*
*    SGE_LIST(JJ_not_yet_finished_ids) - Not Yet Finished Ids
*    Ids of not yet finished tasks.
*
*    SGE_LIST(JJ_started_task_ids) - Started Task Ids
*    Ids of started tasks.
*
*/

enum {
   JJ_jobid = JJ_LOWERBOUND,
   JJ_type,
   JJ_finished_tasks,
   JJ_not_yet_finished_ids,
   JJ_started_task_ids
};

LISTDEF(JJ_Type)
   SGE_ULONG(JJ_jobid, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(JJ_type, CULL_DEFAULT)
   SGE_LIST(JJ_finished_tasks, JJAT_Type, CULL_DEFAULT)
   SGE_LIST(JJ_not_yet_finished_ids, RN_Type, CULL_DEFAULT)
   SGE_LIST(JJ_started_task_ids, RN_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(JJN)
   NAME("JJ_jobid")
   NAME("JJ_type")
   NAME("JJ_finished_tasks")
   NAME("JJ_not_yet_finished_ids")
   NAME("JJ_started_task_ids")
NAMEEND

#define JJ_SIZE sizeof(JJN)/sizeof(char *)


