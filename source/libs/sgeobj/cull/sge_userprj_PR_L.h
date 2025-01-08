#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/PR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Project
*
* An object of this type holds project configuration data as well as usage accumulators.
*
*    SGE_STRING(PR_name) - Project Name
*    A unique name of the project.
*
*    SGE_ULONG(PR_oticket) - Override Tickets
*    Configured override tickets.
*
*    SGE_ULONG(PR_fshare) - Functional Shares
*    Configured functional shares.
*
*    SGE_ULONG(PR_job_cnt) - Job Count
*    Job counter, used in scheduler thread only.
*
*    SGE_ULONG(PR_pending_job_cnt) - Pending Job Count
*    Pending job counter, used in scheduler thread only.
*
*    SGE_LIST(PR_usage) - Usage
*    List of UA_Type objects storing accumulated and decayed usage of jobs belonging to this project.
*
*    SGE_ULONG64(PR_usage_time_stamp) - Usage Time Stamp
*    Time stamp of last decay. Set when usage changes.
*
*    SGE_ULONG(PR_usage_seqno) - Usage Sequence Number
*    Usage sequence number used in scheduler thread only.
*
*    SGE_LIST(PR_long_term_usage) - Long Term Usage
*    List of UA_Type objects holding long term accumulated non-decayed usage.
*
*    SGE_LIST(PR_project) - Project Usage
*    Usage on a project basis (?).
*
*    SGE_LIST(PR_acl) - Access List
*    US_Type but only names are filled configured excluded user access list used.
*
*    SGE_LIST(PR_xacl) - No Access List
*    US_Type but only names are filled configured excluded user access list used.
*
*    SGE_LIST(PR_debited_job_usage) - Debited Job Usage
*    List of UPU_Type, still debited usage per job.
*
*    SGE_ULONG(PR_version) - Project Version
*    project version, increments when usage is updated
*
*    SGE_BOOL(PR_consider_with_categories) - Consider With Categories
*    True, if project plays role with categories.
*
*    SGE_LIST(PR_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   PR_name = PR_LOWERBOUND,
   PR_oticket,
   PR_fshare,
   PR_job_cnt,
   PR_pending_job_cnt,
   PR_usage,
   PR_usage_time_stamp,
   PR_usage_seqno,
   PR_long_term_usage,
   PR_project,
   PR_acl,
   PR_xacl,
   PR_debited_job_usage,
   PR_version,
   PR_consider_with_categories,
   PR_joker
};

LISTDEF(PR_Type)
   SGE_STRING(PR_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_ULONG(PR_oticket, CULL_SPOOL)
   SGE_ULONG(PR_fshare, CULL_SPOOL)
   SGE_ULONG(PR_job_cnt, CULL_DEFAULT)
   SGE_ULONG(PR_pending_job_cnt, CULL_DEFAULT)
   SGE_LIST(PR_usage, UA_Type, CULL_SPOOL)
   SGE_ULONG64(PR_usage_time_stamp, CULL_SPOOL)
   SGE_ULONG(PR_usage_seqno, CULL_DEFAULT)
   SGE_LIST(PR_long_term_usage, UA_Type, CULL_SPOOL)
   SGE_LIST(PR_project, UPP_Type, CULL_SPOOL)
   SGE_LIST(PR_acl, US_Type, CULL_SPOOL_PROJECT)
   SGE_LIST(PR_xacl, US_Type, CULL_SPOOL_PROJECT)
   SGE_LIST(PR_debited_job_usage, UPU_Type, CULL_SPOOL)
   SGE_ULONG(PR_version, CULL_DEFAULT)
   SGE_BOOL(PR_consider_with_categories, CULL_DEFAULT)
   SGE_LIST(PR_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(PRN)
   NAME("PR_name")
   NAME("PR_oticket")
   NAME("PR_fshare")
   NAME("PR_job_cnt")
   NAME("PR_pending_job_cnt")
   NAME("PR_usage")
   NAME("PR_usage_time_stamp")
   NAME("PR_usage_seqno")
   NAME("PR_long_term_usage")
   NAME("PR_project")
   NAME("PR_acl")
   NAME("PR_xacl")
   NAME("PR_debited_job_usage")
   NAME("PR_version")
   NAME("PR_consider_with_categories")
   NAME("PR_joker")
NAMEEND

#define PR_SIZE sizeof(PRN)/sizeof(char *)


