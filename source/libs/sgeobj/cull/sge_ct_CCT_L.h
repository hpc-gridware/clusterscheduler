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
 * This code was generated from file source/libs/sgeobj/json/CCT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Category Cache Entry
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Category Cache Entry
*
* What the scheduler learned about one category while scheduling for one parallel environment.
* Jobs in a category make identical requests, so a queue or host that rejected the first job rejects them all. Remembering those rejections turns a per-job search into a per-category one, which is where most of the scheduler's speed on a large pending list comes from.
*
*    SGE_STRING(CCT_pe_name) - Parallel Environment
*    The parallel environment this cache is for; a category is cached once per PE it was tried with.
*
*    SGE_LIST(CCT_ignore_queues) - Rejected Queues
*    Cluster queues already known not to suit this category (`CTI_Type`), so later jobs skip them.
*
*    SGE_LIST(CCT_ignore_hosts) - Rejected Hosts
*    Hosts already known not to suit this category.
*
*    SGE_LIST(CCT_job_messages) - Scheduler Messages
*    The "why not" messages (`MES_Type`) produced for this category, reused rather than regenerated per job.
*
*    SGE_REF(CCT_pe_job_slots) - Possible Slot Counts
*    Reference to the set of slot counts the parallel environment could grant, cached across the category's jobs.
*
*    SGE_ULONG(CCT_pe_job_slot_count) - Slot Count Size
*    How many entries CCT_pe_job_slots holds.
*
*/

enum {
   CCT_pe_name = CCT_LOWERBOUND,   ///< Parallel Environment
   CCT_ignore_queues,   ///< Rejected Queues
   CCT_ignore_hosts,   ///< Rejected Hosts
   CCT_job_messages,   ///< Scheduler Messages
   CCT_pe_job_slots,   ///< Possible Slot Counts
   CCT_pe_job_slot_count   ///< Slot Count Size
};

LISTDEF(CCT_Type)
   SGE_STRING(CCT_pe_name, CULL_DEFAULT)
   SGE_LIST(CCT_ignore_queues, CTI_Type, CULL_DEFAULT)
   SGE_LIST(CCT_ignore_hosts, CTI_Type, CULL_DEFAULT)
   SGE_LIST(CCT_job_messages, MES_Type, CULL_DEFAULT)
   SGE_REF(CCT_pe_job_slots, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(CCT_pe_job_slot_count, CULL_DEFAULT)
LISTEND

NAMEDEF(CCTN)
   NAME("CCT_pe_name")
   NAME("CCT_ignore_queues")
   NAME("CCT_ignore_hosts")
   NAME("CCT_job_messages")
   NAME("CCT_pe_job_slots")
   NAME("CCT_pe_job_slot_count")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define CCT_SIZE sizeof(CCTN)/sizeof(char *)


