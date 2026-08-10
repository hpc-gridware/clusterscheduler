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
 * This code was generated from file source/libs/sgeobj/json/US.json
 * DO NOT CHANGE
 */

/** @file
 * @brief User Set
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief User Set
*
* A named set of users and groups, used either as an access list or as a department.
* One type serves both: `US_type` says which. As an access list it decides who may use a queue, project or parallel environment; as a department it carries the share-tree and override tickets its members get. A userset may be both at once.
*
*    SGE_STRING(US_name) - Name
*    The set's name. `defaultdepartment` is the fallback department.
*
*    SGE_ULONG(US_type) - Type
*    `US_ACL`, `US_DEPT`, or both. See `sge_userset.h`.
*
*    SGE_ULONG(US_fshare) - Functional Share
*    The department's share of functional tickets.
*
*    SGE_ULONG(US_oticket) - Override Tickets
*    Override tickets granted to the department's jobs.
*
*    SGE_ULONG(US_job_cnt) - Running Job Count
*    Jobs of this userset currently running, maintained by the scheduler for ticket calculation.
*
*    SGE_ULONG(US_pending_job_cnt) - Pending Job Count
*    Jobs of this userset waiting to run.
*
*    SGE_LIST(US_entries) - Members
*    The users and UNIX groups in the set (`UE_Type`). A name starting with `@` is a group.
*
*    SGE_BOOL(US_consider_with_categories) - Category Relevant
*    The set affects which jobs can run where, so two jobs differing only in it must not share a scheduling category.
*
*    SGE_LIST(US_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   US_name = US_LOWERBOUND,   ///< Name
   US_type,   ///< Type
   US_fshare,   ///< Functional Share
   US_oticket,   ///< Override Tickets
   US_job_cnt,   ///< Running Job Count
   US_pending_job_cnt,   ///< Pending Job Count
   US_entries,   ///< Members
   US_consider_with_categories,   ///< Category Relevant
   US_joker   ///< Joker
};

LISTDEF(US_Type)
   SGE_STRING(US_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_ULONG(US_type, CULL_SPOOL)
   SGE_ULONG(US_fshare, CULL_SPOOL)
   SGE_ULONG(US_oticket, CULL_SPOOL)
   SGE_ULONG(US_job_cnt, CULL_DEFAULT)
   SGE_ULONG(US_pending_job_cnt, CULL_DEFAULT)
   SGE_LIST(US_entries, UE_Type, CULL_SPOOL)
   SGE_BOOL(US_consider_with_categories, CULL_DEFAULT)
   SGE_LIST(US_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(USN)
   NAME("US_name")
   NAME("US_type")
   NAME("US_fshare")
   NAME("US_oticket")
   NAME("US_job_cnt")
   NAME("US_pending_job_cnt")
   NAME("US_entries")
   NAME("US_consider_with_categories")
   NAME("US_joker")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define US_SIZE sizeof(USN)/sizeof(char *)


