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
 * This code was generated from file source/libs/sgeobj/json/RUE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Utilization
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of RUE
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   RUE_name = 10600,   ///< Resource Name
   RUE_utilized_now,   ///< Utilized Now
   RUE_utilized_now_resource_map_list,   ///< Utilized Now Resource Map List
   RUE_utilized_now_binding_inuse,   ///< Utilized Now Binding In Use
   RUE_utilized,   ///< Utilized
   RUE_utilized_now_nonexclusive,   ///< Utilized Now Non-Exclusive
   RUE_utilized_nonexclusive   ///< Utilized Non-Exclusive
};

/** @brief The attribute ids of RUE, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int RUE_Type[] = {
   RUE_name,
   RUE_utilized_now,
   RUE_utilized_now_resource_map_list,
   RUE_utilized_now_binding_inuse,
   RUE_utilized,
   RUE_utilized_now_nonexclusive,
   RUE_utilized_nonexclusive,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of RUE
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define RUE_ATTRIBUTES \
   {RUE_name, "RUE_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {RUE_utilized_now, "RUE_utilized_now", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized_now_resource_map_list, "RUE_utilized_now_resource_map_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized_now_binding_inuse, "RUE_utilized_now_binding_inuse", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized, "RUE_utilized", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized_now_nonexclusive, "RUE_utilized_now_nonexclusive", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized_nonexclusive, "RUE_utilized_nonexclusive", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

