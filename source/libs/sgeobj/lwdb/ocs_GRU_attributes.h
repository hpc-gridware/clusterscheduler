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
 * This code was generated from file source/libs/sgeobj/json/GRU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Granted Resource
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of GRU
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   GRU_type = 3650,   ///< Type
   GRU_name,   ///< Name
   GRU_amount,   ///< Granted Amount
   GRU_resource_map_list,   ///< Resource Map List
   GRU_binding_inuse,   ///< Grant Binding In Use
   GRU_host   ///< Host
};

/** @brief The attribute ids of GRU, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int GRU_Type[] = {
   GRU_type,
   GRU_name,
   GRU_amount,
   GRU_resource_map_list,
   GRU_binding_inuse,
   GRU_host,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of GRU
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define GRU_ATTRIBUTES \
   {GRU_type, "GRU_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {GRU_name, "GRU_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {GRU_amount, "GRU_amount", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {GRU_resource_map_list, "GRU_resource_map_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {GRU_binding_inuse, "GRU_binding_inuse", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {GRU_host, "GRU_host", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

