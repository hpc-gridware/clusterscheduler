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
 * This code was generated from file source/libs/sgeobj/json/SCT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(SCT_str) - @todo add summary
*    @todo add description
*
*    SGE_LIST(SCT_job_pending_ref) - @todo add summary
*    @todo add description
*
*    SGE_LIST(SCT_job_ref) - @todo add summary
*    @todo add description
*
*/

enum {
   SCT_str = SCT_LOWERBOUND,
   SCT_job_pending_ref,
   SCT_job_ref
};

LISTDEF(SCT_Type)
   SGE_STRING(SCT_str, CULL_UNIQUE | CULL_HASH)
   SGE_LIST(SCT_job_pending_ref, REF_Type, CULL_DEFAULT)
   SGE_LIST(SCT_job_ref, REF_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(SCTN)
   NAME("SCT_str")
   NAME("SCT_job_pending_ref")
   NAME("SCT_job_ref")
NAMEEND

#define SCT_SIZE sizeof(SCTN)/sizeof(char *)


