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
 * This code was generated from file source/libs/sgeobj/json/SGEJ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Sort Object
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of SGEJ
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   SGEJ_priority = 6750,   ///< Priority
   SGEJ_job_number,   ///< Job Number
   SGEJ_job_name,   ///< Job Name
   SGEJ_owner,   ///< Owner
   SGEJ_state,   ///< State
   SGEJ_master_queue,   ///< Master Queue
   SGEJ_job_reference,   ///< Job Reference
   SGEJ_submission_time   ///< Submission Time
};

/** @brief The attribute ids of SGEJ, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int SGEJ_Type[] = {
   SGEJ_priority,
   SGEJ_job_number,
   SGEJ_job_name,
   SGEJ_owner,
   SGEJ_state,
   SGEJ_master_queue,
   SGEJ_job_reference,
   SGEJ_submission_time,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of SGEJ
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define SGEJ_ATTRIBUTES \
   {SGEJ_priority, "SGEJ_priority", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SGEJ_job_number, "SGEJ_job_number", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SGEJ_job_name, "SGEJ_job_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SGEJ_owner, "SGEJ_owner", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SGEJ_state, "SGEJ_state", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SGEJ_master_queue, "SGEJ_master_queue", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SGEJ_job_reference, "SGEJ_job_reference", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SGEJ_submission_time, "SGEJ_submission_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

