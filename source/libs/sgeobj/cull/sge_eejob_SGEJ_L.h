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
 * This code was generated from file source/libs/sgeobj/json/SGEJ.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_DOUBLE(SGEJ_priority) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SGEJ_job_number) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SGEJ_job_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SGEJ_owner) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SGEJ_state) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SGEJ_master_queue) - @todo add summary
*    @todo add description
*
*    SGE_REF(SGEJ_job_reference) - @todo add summary
*    @todo add description
*
*    SGE_ULONG64(SGEJ_submission_time) - @todo add summary
*    @todo add description
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


