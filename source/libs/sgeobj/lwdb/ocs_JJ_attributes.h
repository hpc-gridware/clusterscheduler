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
 * This code was generated from file source/libs/sgeobj/json/JJ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief JAPI Job
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of JJ
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   JJ_jobid = 7950,   ///< Job Id
   JJ_type,   ///< Type
   JJ_finished_tasks,   ///< Finished Tasks
   JJ_not_yet_finished_ids,   ///< Not Yet Finished Ids
   JJ_started_task_ids   ///< Started Task Ids
};

/** @brief The attribute ids of JJ, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int JJ_Type[] = {
   JJ_jobid,
   JJ_type,
   JJ_finished_tasks,
   JJ_not_yet_finished_ids,
   JJ_started_task_ids,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of JJ
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define JJ_ATTRIBUTES \
   {JJ_jobid, "JJ_jobid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, false}, \
   {JJ_type, "JJ_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JJ_finished_tasks, "JJ_finished_tasks", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JJ_not_yet_finished_ids, "JJ_not_yet_finished_ids", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JJ_started_task_ids, "JJ_started_task_ids", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

