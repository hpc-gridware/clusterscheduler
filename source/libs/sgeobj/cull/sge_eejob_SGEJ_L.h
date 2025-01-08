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
 * This code was generated from file source/libs/sgeobj/json/SGEJ.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Sort Object
*
* This is the list type we use to sort the joblist in the sge scheduler.
* The list will be build before the actual scheduling.
* It is sorted by priorty ASC, submission time ASC, job number DESC.
* Jobs in this list will then be scheduled one after the other.
*
*    SGE_DOUBLE(SGEJ_priority) - Priority
*    Normalized priority of the job/array task after applying all policies.
*    The job list is sorted by priority descending as primary sort criterion.
*
*    SGE_ULONG(SGEJ_job_number) - Job Number
*    The job number. Third sort criterion.
*
*    SGE_STRING(SGEJ_job_name) - Job Name
*    The job name.
*    @todo can be removed, it is only set, never read.
*
*    SGE_STRING(SGEJ_owner) - Owner
*    The job owner.
*    @todo can be removed, it is only set, never read.
*
*    SGE_ULONG(SGEJ_state) - State
*    The state of an enrolled array task (JAT_state).
*    @todo can be removed, it is only set, never read.
*
*    SGE_STRING(SGEJ_master_queue) - Master Queue
*    The master queue of an enrolled array task (JAT_master_queue).
*    @todo can be removed, it is only set, never read.
*
*    SGE_REF(SGEJ_job_reference) - Job Reference
*    Reference (Pointer) to the job in the scheduler's job list.
*
*    SGE_ULONG64(SGEJ_submission_time) - Submission Time
*    The job submission time. Secondary sort criterion.
*
*/

enum {
   SGEJ_priority = SGEJ_LOWERBOUND,
   SGEJ_job_number,
   SGEJ_job_name,
   SGEJ_owner,
   SGEJ_state,
   SGEJ_master_queue,
   SGEJ_job_reference,
   SGEJ_submission_time
};

LISTDEF(SGEJ_Type)
   SGE_DOUBLE(SGEJ_priority, CULL_DEFAULT)
   SGE_ULONG(SGEJ_job_number, CULL_DEFAULT)
   SGE_STRING(SGEJ_job_name, CULL_DEFAULT)
   SGE_STRING(SGEJ_owner, CULL_DEFAULT)
   SGE_ULONG(SGEJ_state, CULL_DEFAULT)
   SGE_STRING(SGEJ_master_queue, CULL_DEFAULT)
   SGE_REF(SGEJ_job_reference, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG64(SGEJ_submission_time, CULL_DEFAULT)
LISTEND

NAMEDEF(SGEJN)
   NAME("SGEJ_priority")
   NAME("SGEJ_job_number")
   NAME("SGEJ_job_name")
   NAME("SGEJ_owner")
   NAME("SGEJ_state")
   NAME("SGEJ_master_queue")
   NAME("SGEJ_job_reference")
   NAME("SGEJ_submission_time")
NAMEEND

#define SGEJ_SIZE sizeof(SGEJN)/sizeof(char *)


