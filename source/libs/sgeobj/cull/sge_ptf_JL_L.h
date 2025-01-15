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
 * This code was generated from file source/libs/sgeobj/json/JL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief PTF Job
*
* This is the list type we use to hold the jobs which are running under the local execution daemon.
* @todo This is a very old module and probably noone really knows its details any more. Add better documentation.
*
*    SGE_ULONG(JL_job_ID) - Job Id
*    The Job Id.
*
*    SGE_LIST(JL_OS_job_list) - OS Job List
*    List of OS jobs (JO_Type). There should be one per running Array/PE task having one os job id each.
*
*    SGE_ULONG(JL_state) - State
*    Job State, one of JL_JOB_ACTIVE, JL_JOB_COMPLETE, JL_JOB_DELETED.
*
*    SGE_ULONG(JL_tickets) - Tickets
*    Job Tickets. Used for reprioritization.
*
*    SGE_DOUBLE(JL_share) - Share
*    ptf interval share
*
*    SGE_DOUBLE(JL_ticket_share) - Ticket Share
*    ptf job ticket share
*
*    SGE_DOUBLE(JL_timeslice) - Timeslice
*    ptf calculated timeslice (SX)
*
*    SGE_DOUBLE(JL_usage) - Usage
*    ptf interval combined usage
*
*    SGE_DOUBLE(JL_old_usage_value) - Old Usage
*    ptf interval combined usage
*
*    SGE_DOUBLE(JL_adjusted_usage) - Adjusted Usage
*    ptf interval adjusted usage
*
*    SGE_DOUBLE(JL_last_usage) - Last Usage
*    ptf last interval combined usage
*
*    SGE_DOUBLE(JL_old_usage) - Old Usage
*    prev interval combined usage
*
*    SGE_DOUBLE(JL_proportion) - Proportion
*    @todo add description
*
*    SGE_DOUBLE(JL_adjusted_proportion) - Adjusted Proportion
*    @todo add description
*
*    SGE_DOUBLE(JL_adjusted_current_proportion) - Adjusted Current Proportion
*    @todo add description
*
*    SGE_DOUBLE(JL_actual_proportion) - Actual Proportion
*    @todo add description
*
*    SGE_DOUBLE(JL_diff_proportion) - Diff Proportion
*    @todo add description
*
*    SGE_DOUBLE(JL_last_proportion) - Last Proportion
*    @todo add description
*
*    SGE_DOUBLE(JL_curr_pri) - Current Priority
*    @todo add description
*
*    SGE_LONG(JL_pri) - Priority
*    @todo add description
*
*    SGE_ULONG(JL_procfd) - Proc FD
*    proc file descriptor
*
*    SGE_ULONG(JL_interactive) - Interactive
*    interactive flag
*
*/

enum {
   JL_job_ID = JL_LOWERBOUND,
   JL_OS_job_list,
   JL_state,
   JL_tickets,
   JL_share,
   JL_ticket_share,
   JL_timeslice,
   JL_usage,
   JL_old_usage_value,
   JL_adjusted_usage,
   JL_last_usage,
   JL_old_usage,
   JL_proportion,
   JL_adjusted_proportion,
   JL_adjusted_current_proportion,
   JL_actual_proportion,
   JL_diff_proportion,
   JL_last_proportion,
   JL_curr_pri,
   JL_pri,
   JL_procfd,
   JL_interactive
};

LISTDEF(JL_Type)
   SGE_ULONG(JL_job_ID, CULL_DEFAULT)
   SGE_LIST(JL_OS_job_list, JO_Type, CULL_DEFAULT)
   SGE_ULONG(JL_state, CULL_DEFAULT)
   SGE_ULONG(JL_tickets, CULL_DEFAULT)
   SGE_DOUBLE(JL_share, CULL_DEFAULT)
   SGE_DOUBLE(JL_ticket_share, CULL_DEFAULT)
   SGE_DOUBLE(JL_timeslice, CULL_DEFAULT)
   SGE_DOUBLE(JL_usage, CULL_DEFAULT)
   SGE_DOUBLE(JL_old_usage_value, CULL_DEFAULT)
   SGE_DOUBLE(JL_adjusted_usage, CULL_DEFAULT)
   SGE_DOUBLE(JL_last_usage, CULL_DEFAULT)
   SGE_DOUBLE(JL_old_usage, CULL_DEFAULT)
   SGE_DOUBLE(JL_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_adjusted_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_adjusted_current_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_actual_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_diff_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_last_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_curr_pri, CULL_DEFAULT)
   SGE_LONG(JL_pri, CULL_DEFAULT)
   SGE_ULONG(JL_procfd, CULL_DEFAULT)
   SGE_ULONG(JL_interactive, CULL_DEFAULT)
LISTEND

NAMEDEF(JLN)
   NAME("JL_job_ID")
   NAME("JL_OS_job_list")
   NAME("JL_state")
   NAME("JL_tickets")
   NAME("JL_share")
   NAME("JL_ticket_share")
   NAME("JL_timeslice")
   NAME("JL_usage")
   NAME("JL_old_usage_value")
   NAME("JL_adjusted_usage")
   NAME("JL_last_usage")
   NAME("JL_old_usage")
   NAME("JL_proportion")
   NAME("JL_adjusted_proportion")
   NAME("JL_adjusted_current_proportion")
   NAME("JL_actual_proportion")
   NAME("JL_diff_proportion")
   NAME("JL_last_proportion")
   NAME("JL_curr_pri")
   NAME("JL_pri")
   NAME("JL_procfd")
   NAME("JL_interactive")
NAMEEND

#define JL_SIZE sizeof(JLN)/sizeof(char *)


