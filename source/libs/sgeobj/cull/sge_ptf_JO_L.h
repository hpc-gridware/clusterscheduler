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
 * This code was generated from file source/libs/sgeobj/json/JO.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(JO_OS_job_ID) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JO_OS_job_ID2) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JO_ja_task_ID) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JO_task_id_str) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JO_state) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JO_usage_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JO_pid_list) - @todo add summary
*    @todo add description
*
*/

enum {
   JO_OS_job_ID = JO_LOWERBOUND,
   JO_OS_job_ID2,
   JO_ja_task_ID,
   JO_task_id_str,
   JO_state,
   JO_usage_list,
   JO_pid_list
};

LISTDEF(JO_Type)
   SGE_ULONG(JO_OS_job_ID, CULL_DEFAULT)
   SGE_ULONG(JO_OS_job_ID2, CULL_DEFAULT)
   SGE_ULONG(JO_ja_task_ID, CULL_DEFAULT)
   SGE_STRING(JO_task_id_str, CULL_DEFAULT)
   SGE_ULONG(JO_state, CULL_DEFAULT)
   SGE_LIST(JO_usage_list, UA_Type, CULL_DEFAULT)
   SGE_LIST(JO_pid_list, JP_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(JON)
   NAME("JO_OS_job_ID")
   NAME("JO_OS_job_ID2")
   NAME("JO_ja_task_ID")
   NAME("JO_task_id_str")
   NAME("JO_state")
   NAME("JO_usage_list")
   NAME("JO_pid_list")
NAMEEND

#define JO_SIZE sizeof(JON)/sizeof(char *)


