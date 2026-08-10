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
 * This code was generated from file source/libs/sgeobj/json/LDR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Load Threshold Reference (unused)
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of LDR
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   LDR_queue_ref_list = 10800,   ///< Queue References
   LDR_limit,   ///< Limit
   LDR_global,   ///< Global Object
   LDR_host,   ///< Host Object
   LDR_queue   ///< Queue Object
};

/** @brief The attribute ids of LDR, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int LDR_Type[] = {
   LDR_queue_ref_list,
   LDR_limit,
   LDR_global,
   LDR_host,
   LDR_queue,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of LDR
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define LDR_ATTRIBUTES \
   {LDR_queue_ref_list, "LDR_queue_ref_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LDR_limit, "LDR_limit", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LDR_global, "LDR_global", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LDR_host, "LDR_host", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LDR_queue, "LDR_queue", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

