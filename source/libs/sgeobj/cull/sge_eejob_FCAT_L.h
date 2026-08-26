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
 * This code was generated from file source/libs/sgeobj/json/FCAT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Functional Category
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Functional Category
*
* Objects of this type are used to sort the job list in the scheduler.
* All attributes are references to objects in other lists.
*
*    SGE_ULONG(FCAT_job_share) - Job Share
*    All jobs in this functional category have this amount of jobs shares.
*
*    SGE_ULONG(FCAT_user_share) - User Share
*    All jobs in this functional category have this amount of user shares.
*
*    SGE_REF(FCAT_user) - User
*    Pointer to the user structure.
*
*    SGE_ULONG(FCAT_project_share) - Project Share
*    All jobs in this functional category have this amount of project shares.
*
*    SGE_REF(FCAT_project) - Project
*    Pointer to the project structure.
*
*    SGE_ULONG(FCAT_dept_share) - Department Share
*    All jobs in this functional category have this amount of department shares.
*
*    SGE_REF(FCAT_dept) - Department
*    Pointer to the department structure.
*
*    SGE_REF(FCAT_jobrelated_ticket_first) - First Job Related Ticket
*    Pointer to the first element of job ticket list.
*
*    SGE_REF(FCAT_jobrelated_ticket_last) - Last Job Related Ticket
*    Pointer to the last element in the job ticket list.
*
*/

enum {
   FCAT_job_share = FCAT_LOWERBOUND,   ///< Job Share
   FCAT_user_share,   ///< User Share
   FCAT_user,   ///< User
   FCAT_project_share,   ///< Project Share
   FCAT_project,   ///< Project
   FCAT_dept_share,   ///< Department Share
   FCAT_dept,   ///< Department
   FCAT_jobrelated_ticket_first,   ///< First Job Related Ticket
   FCAT_jobrelated_ticket_last   ///< Last Job Related Ticket
};

LISTDEF(FCAT_Type)
   SGE_ULONG(FCAT_job_share, CULL_DEFAULT)
   SGE_ULONG(FCAT_user_share, CULL_DEFAULT)
   SGE_REF(FCAT_user, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_ULONG(FCAT_project_share, CULL_DEFAULT)
   SGE_REF(FCAT_project, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_ULONG(FCAT_dept_share, CULL_DEFAULT)
   SGE_REF(FCAT_dept, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_REF(FCAT_jobrelated_ticket_first, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
   SGE_REF(FCAT_jobrelated_ticket_last, CULL_ANY_SUBTYPE, CULL_NO_TRANSFER)
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

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define FCAT_SIZE sizeof(FCATN)/sizeof(char *)


