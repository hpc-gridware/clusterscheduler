#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/RQL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Scheduling Cache
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource Quota Scheduling Cache
*
* Per-scheduling-run cache of what a resource quota limit answered, so an expensive limit is evaluated once rather than once per queue instance.
* Not configuration and not spooled: it lives in the scheduler's assignment structure for the duration of one run.
*
*    SGE_STRING(RQL_name) - Limit Key
*    Identifies the limit this entry caches, built from the rule and resource being evaluated.
*
*    SGE_INT(RQL_result) - Cached Result
*    The `dispatch_t` the limit returned last time it was asked in this run.
*
*    SGE_ULONG64(RQL_time) - Earliest Time
*    Earliest time the limit would allow the job to start, for reservation scheduling.
*
*    SGE_INT(RQL_slots) - Slots
*    How many slots the limit allows, cached alongside the result.
*
*    SGE_ULONG(RQL_tagged4schedule) - Tagged For Schedule
*    Marks the entry as belonging to the run in progress, so stale entries are not believed.
*
*/

enum {
   RQL_name = RQL_LOWERBOUND,   ///< Limit Key
   RQL_result,   ///< Cached Result
   RQL_time,   ///< Earliest Time
   RQL_slots,   ///< Slots
   RQL_tagged4schedule   ///< Tagged For Schedule
};

LISTDEF(RQL_Type)
   SGE_STRING(RQL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH)
   SGE_INT(RQL_result, CULL_DEFAULT)
   SGE_ULONG64(RQL_time, CULL_DEFAULT)
   SGE_INT(RQL_slots, CULL_DEFAULT)
   SGE_ULONG(RQL_tagged4schedule, CULL_DEFAULT)
LISTEND

NAMEDEF(RQLN)
   NAME("RQL_name")
   NAME("RQL_result")
   NAME("RQL_time")
   NAME("RQL_slots")
   NAME("RQL_tagged4schedule")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define RQL_SIZE sizeof(RQLN)/sizeof(char *)


