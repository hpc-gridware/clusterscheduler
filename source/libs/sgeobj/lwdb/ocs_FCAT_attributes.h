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

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of FCAT
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   FCAT_job_share = 9800,   ///< Job Share
   FCAT_user_share,   ///< User Share
   FCAT_user,   ///< User
   FCAT_project_share,   ///< Project Share
   FCAT_project,   ///< Project
   FCAT_dept_share,   ///< Department Share
   FCAT_dept,   ///< Department
   FCAT_jobrelated_ticket_first,   ///< First Job Related Ticket
   FCAT_jobrelated_ticket_last   ///< Last Job Related Ticket
};

/** @brief The attribute ids of FCAT, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int FCAT_Type[] = {
   FCAT_job_share,
   FCAT_user_share,
   FCAT_user,
   FCAT_project_share,
   FCAT_project,
   FCAT_dept_share,
   FCAT_dept,
   FCAT_jobrelated_ticket_first,
   FCAT_jobrelated_ticket_last,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of FCAT
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define FCAT_ATTRIBUTES \
   {FCAT_job_share, "FCAT_job_share", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {FCAT_user_share, "FCAT_user_share", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {FCAT_user, "FCAT_user", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {FCAT_project_share, "FCAT_project_share", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {FCAT_project, "FCAT_project", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {FCAT_dept_share, "FCAT_dept_share", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {FCAT_dept, "FCAT_dept", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {FCAT_jobrelated_ticket_first, "FCAT_jobrelated_ticket_first", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {FCAT_jobrelated_ticket_last, "FCAT_jobrelated_ticket_last", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

