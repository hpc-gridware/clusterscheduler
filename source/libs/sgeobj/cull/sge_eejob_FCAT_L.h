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
 * This code was generated from file source/libs/sgeobj/json/FCAT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(FCAT_job_share) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(FCAT_user_share) - @todo add summary
*    @todo add description
*
*    SGE_REF(FCAT_user) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(FCAT_project_share) - @todo add summary
*    @todo add description
*
*    SGE_REF(FCAT_project) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(FCAT_dept_share) - @todo add summary
*    @todo add description
*
*    SGE_REF(FCAT_dept) - @todo add summary
*    @todo add description
*
*    SGE_REF(FCAT_jobrelated_ticket_first) - @todo add summary
*    @todo add description
*
*    SGE_REF(FCAT_jobrelated_ticket_last) - @todo add summary
*    @todo add description
*
*/

enum {
   FCAT_job_share = FCAT_LOWERBOUND,
   FCAT_user_share,
   FCAT_user,
   FCAT_project_share,
   FCAT_project,
   FCAT_dept_share,
   FCAT_dept,
   FCAT_jobrelated_ticket_first,
   FCAT_jobrelated_ticket_last
};

LISTDEF(FCAT_Type)
   SGE_ULONG(FCAT_job_share, CULL_DEFAULT)
   SGE_ULONG(FCAT_user_share, CULL_DEFAULT)
   SGE_REF(FCAT_user, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(FCAT_project_share, CULL_DEFAULT)
   SGE_REF(FCAT_project, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(FCAT_dept_share, CULL_DEFAULT)
   SGE_REF(FCAT_dept, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(FCAT_jobrelated_ticket_first, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(FCAT_jobrelated_ticket_last, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(FCATN)
   NAME("FCAT_job_share")
   NAME("FCAT_user_share")
   NAME("FCAT_user")
   NAME("FCAT_project_share")
   NAME("FCAT_project")
   NAME("FCAT_dept_share")
   NAME("FCAT_dept")
   NAME("FCAT_jobrelated_ticket_first")
   NAME("FCAT_jobrelated_ticket_last")
NAMEEND

#define FCAT_SIZE sizeof(FCATN)/sizeof(char *)


