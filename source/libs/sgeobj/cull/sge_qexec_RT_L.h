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
 * This code was generated from file source/libs/sgeobj/json/RT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Remote Task
*
* An object of this type represents a remote task.
* It is used in qrsh -inherit starting tasks within a tightly integrated parallel job.
*
*    SGE_STRING(RT_tid) - Task Id
*    Task Id.
*
*    SGE_HOST(RT_hostname) - Hostname
*    Remote host where the task is running on.
*
*    SGE_ULONG(RT_status) - Status
*    Status as it comes from waitpid(2).
*
*    SGE_ULONG(RT_state) - State
*    Internal state used by the qrexec module.
*
*/

enum {
   RT_tid = RT_LOWERBOUND,
   RT_hostname,
   RT_status,
   RT_state
};

LISTDEF(RT_Type)
   SGE_STRING(RT_tid, CULL_DEFAULT)
   SGE_HOST(RT_hostname, CULL_DEFAULT)
   SGE_ULONG(RT_status, CULL_DEFAULT)
   SGE_ULONG(RT_state, CULL_DEFAULT)
LISTEND

NAMEDEF(RTN)
   NAME("RT_tid")
   NAME("RT_hostname")
   NAME("RT_status")
   NAME("RT_state")
NAMEEND

#define RT_SIZE sizeof(RTN)/sizeof(char *)


