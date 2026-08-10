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
 * This code was generated from file source/libs/sgeobj/json/SPT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Spooling Type
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of SPT
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   SPT_type = 7750,   ///< Type
   SPT_name,   ///< Name
   SPT_rules   ///< Rules
};

/** @brief The attribute ids of SPT, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int SPT_Type[] = {
   SPT_type,
   SPT_name,
   SPT_rules,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of SPT
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define SPT_ATTRIBUTES \
   {SPT_type, "SPT_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, false}, \
   {SPT_name, "SPT_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {SPT_rules, "SPT_rules", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

