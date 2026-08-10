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
 * This code was generated from file source/libs/sgeobj/json/SO.json
 * DO NOT CHANGE
 */

/** @file
 * @brief SubordinateQueue
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of SO
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   SO_name = 2950,   ///< Subordinate Queue Name
   SO_threshold,   ///< Threshold
   SO_slots_sum,   ///< Slots Sum
   SO_seq_no,   ///< Sequence Number
   SO_action   ///< Action
};

/** @brief The attribute ids of SO, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int SO_Type[] = {
   SO_name,
   SO_threshold,
   SO_slots_sum,
   SO_seq_no,
   SO_action,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of SO
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define SO_ATTRIBUTES \
   {SO_name, "SO_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, true, false}, \
   {SO_threshold, "SO_threshold", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SO_slots_sum, "SO_slots_sum", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SO_seq_no, "SO_seq_no", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SO_action, "SO_action", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

