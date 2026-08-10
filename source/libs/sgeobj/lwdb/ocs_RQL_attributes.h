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
 * This code was generated from file source/libs/sgeobj/json/RQL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief @todo add summary
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of RQL
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   RQL_name = 11800,   ///< @todo add summary
   RQL_result,   ///< @todo add summary
   RQL_time,   ///< @todo add summary
   RQL_slots,   ///< @todo add summary
   RQL_tagged4schedule   ///< @todo add summary
};

/** @brief The attribute ids of RQL, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int RQL_Type[] = {
   RQL_name,
   RQL_result,
   RQL_time,
   RQL_slots,
   RQL_tagged4schedule,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of RQL
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define RQL_ATTRIBUTES \
   {RQL_name, "RQL_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {RQL_result, "RQL_result", AttributeStatic::INT, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RQL_time, "RQL_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RQL_slots, "RQL_slots", AttributeStatic::INT, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RQL_tagged4schedule, "RQL_tagged4schedule", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

