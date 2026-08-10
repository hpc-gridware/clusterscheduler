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
 * This code was generated from file source/libs/sgeobj/json/SCT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Scheduler Category Reference (unused)
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Scheduler Category Reference (unused)
*
* A category string with the pending and running jobs that belong to it.
* @warning No attribute of this type is read or written anywhere in either repository, and `SCT_Type` itself is referenced nowhere. Left in place because removing a type changes the spooled object model.
*
*    SGE_STRING(SCT_str) - Category String
*    Part of the unused SCT object; see the object description.
*
*    SGE_LIST(SCT_job_pending_ref) - Pending Jobs
*    Part of the unused SCT object; see the object description.
*
*    SGE_LIST(SCT_job_ref) - Running Jobs
*    Part of the unused SCT object; see the object description.
*
*/

enum {
   SCT_str = SCT_LOWERBOUND,   ///< Category String
   SCT_job_pending_ref,   ///< Pending Jobs
   SCT_job_ref   ///< Running Jobs
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

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define SCT_SIZE sizeof(SCTN)/sizeof(char *)


