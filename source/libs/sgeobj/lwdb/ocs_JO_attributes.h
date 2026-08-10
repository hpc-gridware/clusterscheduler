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
 * This code was generated from file source/libs/sgeobj/json/JO.json
 * DO NOT CHANGE
 */

/** @file
 * @brief OS Job
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of JO
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   JO_OS_job_ID = 4750,   ///< OS Job Id
   JO_OS_job_ID2,   ///< OS Job Id 2
   JO_systemd_scope,   ///< Systemd Scope
   JO_usage_collection,   ///< Usage Collection Mode
   JO_ja_task_ID,   ///< Array Task Id
   JO_task_id_str,   ///< Task Id String
   JO_state,   ///< State
   JO_usage_list,   ///< Usage List
   JO_pid_list   ///< Pid List
};

/** @brief The attribute ids of JO, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int JO_Type[] = {
   JO_OS_job_ID,
   JO_OS_job_ID2,
   JO_systemd_scope,
   JO_usage_collection,
   JO_ja_task_ID,
   JO_task_id_str,
   JO_state,
   JO_usage_list,
   JO_pid_list,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of JO
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define JO_ATTRIBUTES \
   {JO_OS_job_ID, "JO_OS_job_ID", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JO_OS_job_ID2, "JO_OS_job_ID2", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JO_systemd_scope, "JO_systemd_scope", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JO_usage_collection, "JO_usage_collection", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JO_ja_task_ID, "JO_ja_task_ID", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JO_task_id_str, "JO_task_id_str", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JO_state, "JO_state", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JO_usage_list, "JO_usage_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JO_pid_list, "JO_pid_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

