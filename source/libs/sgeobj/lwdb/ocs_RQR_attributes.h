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
 * This code was generated from file source/libs/sgeobj/json/RQR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Rule
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of RQR
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   RQR_name = 11500,   ///< Name
   RQR_filter_users,   ///< User Filter
   RQR_filter_projects,   ///< Project Filter
   RQR_filter_pes,   ///< Parallel Environment Filter
   RQR_filter_queues,   ///< Queue Filter
   RQR_filter_hosts,   ///< Host Filter
   RQR_limit,   ///< Limits
   RQR_level   ///< Limit Level
};

/** @brief The attribute ids of RQR, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int RQR_Type[] = {
   RQR_name,
   RQR_filter_users,
   RQR_filter_projects,
   RQR_filter_pes,
   RQR_filter_queues,
   RQR_filter_hosts,
   RQR_limit,
   RQR_level,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of RQR
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define RQR_ATTRIBUTES \
   {RQR_name, "RQR_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {RQR_filter_users, "RQR_filter_users", AttributeStatic::OBJECT, nullptr, 0, AttributeStatic::NO_HASH, false, true}, \
   {RQR_filter_projects, "RQR_filter_projects", AttributeStatic::OBJECT, nullptr, 1, AttributeStatic::NO_HASH, false, true}, \
   {RQR_filter_pes, "RQR_filter_pes", AttributeStatic::OBJECT, nullptr, 2, AttributeStatic::NO_HASH, false, true}, \
   {RQR_filter_queues, "RQR_filter_queues", AttributeStatic::OBJECT, nullptr, 3, AttributeStatic::NO_HASH, false, true}, \
   {RQR_filter_hosts, "RQR_filter_hosts", AttributeStatic::OBJECT, nullptr, 4, AttributeStatic::NO_HASH, false, true}, \
   {RQR_limit, "RQR_limit", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQR_level, "RQR_level", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

