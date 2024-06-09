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
 * This code was generated from file source/libs/sgeobj/json/UU.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief User
*
* An object of this type holds user configuration data as well as usage accumulators.
*
*    SGE_STRING(UU_name) - User Name
*    A unique name of the user.
*
*    SGE_ULONG(UU_oticket) - Override Tickets
*    Configured override tickets.
*
*    SGE_ULONG(UU_fshare) - Functional Shares
*    Configured functional shares.
*
*    SGE_ULONG(UU_delete_time) - Delete Time
*    If an user submits a job for whom no user object exists, a user object is created.
*    This user object will automatically be deleted at the delete time.
*    Whenever a user submits a job the delete time is updated.
*    When set to 0 (via qconf) the user object is kept forever.
*
*    SGE_ULONG(UU_job_cnt) - Job Count
*    Job counter, used in scheduler thread only.
*
*    SGE_ULONG(UU_pending_job_cnt) - Pending Job Count
*    Pending job counter, used in scheduler thread only.
*
*    SGE_LIST(UU_usage) - Usage
*    List of UA_Type objects storing accumulated and decayed usage of jobs belonging to this user.
*
*    SGE_ULONG(UU_usage_time_stamp) - Usage Time Stamp
*    Time stamp of last decay. Set when usage changes.
*
*    SGE_ULONG(UU_usage_seqno) - Usage Sequence Number
*    Usage sequence number used in scheduler thread only.
*
*    SGE_LIST(UU_long_term_usage) - Long Term Usage
*    List of UA_Type objects holding long term accumulated non-decayed usage.
*
*    SGE_LIST(UU_project) - Project
*    UPP_Type list @todo add description.
*
*    SGE_LIST(UU_debited_job_usage) - Debited Job Usage
*    List of UPU_Type, still debited usage per job.
*
*    SGE_STRING(UU_default_project) - Default Project
*    The default project is the project to which jobs of the user are assigned
*    if no project is specified at job submission time.
*
*    SGE_ULONG(UU_version) - User Version
*    Version of the user object, increments when usage is updated
*
*    SGE_BOOL(UU_consider_with_categories) - Consider With Categories
*    True, if user plays role with categories.
*
*/

enum {
   UU_name = UU_LOWERBOUND,
   UU_oticket,
   UU_fshare,
   UU_delete_time,
   UU_job_cnt,
   UU_pending_job_cnt,
   UU_usage,
   UU_usage_time_stamp,
   UU_usage_seqno,
   UU_long_term_usage,
   UU_project,
   UU_debited_job_usage,
   UU_default_project,
   UU_version,
   UU_consider_with_categories
};

LISTDEF(UU_Type)
   SGE_STRING(UU_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_ULONG(UU_oticket, CULL_SPOOL)
   SGE_ULONG(UU_fshare, CULL_SPOOL)
   SGE_ULONG(UU_delete_time, CULL_SPOOL)
   SGE_ULONG(UU_job_cnt, CULL_DEFAULT)
   SGE_ULONG(UU_pending_job_cnt, CULL_DEFAULT)
   SGE_LIST(UU_usage, UA_Type, CULL_SPOOL)
   SGE_ULONG(UU_usage_time_stamp, CULL_SPOOL)
   SGE_ULONG(UU_usage_seqno, CULL_DEFAULT)
   SGE_LIST(UU_long_term_usage, UA_Type, CULL_SPOOL)
   SGE_LIST(UU_project, UPP_Type, CULL_SPOOL)
   SGE_LIST(UU_debited_job_usage, UPU_Type, CULL_SPOOL)
   SGE_STRING(UU_default_project, CULL_SPOOL)
   SGE_ULONG(UU_version, CULL_DEFAULT)
   SGE_BOOL(UU_consider_with_categories, CULL_DEFAULT)
LISTEND

NAMEDEF(UUN)
   NAME("UU_name")
   NAME("UU_oticket")
   NAME("UU_fshare")
   NAME("UU_delete_time")
   NAME("UU_job_cnt")
   NAME("UU_pending_job_cnt")
   NAME("UU_usage")
   NAME("UU_usage_time_stamp")
   NAME("UU_usage_seqno")
   NAME("UU_long_term_usage")
   NAME("UU_project")
   NAME("UU_debited_job_usage")
   NAME("UU_default_project")
   NAME("UU_version")
   NAME("UU_consider_with_categories")
NAMEEND

#define UU_SIZE sizeof(UUN)/sizeof(char *)


