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
 * This code was generated from file source/libs/sgeobj/json/RESL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource List
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of RESL
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   RESL_value = 3750,   ///< Value
   RESL_id_instance,   ///< ID Instance
   RESL_pe_task_id,   ///< PE Task ID
   RESL_amount,   ///< Resource Amount
   RESL_properties,   ///< Resource Properties
   RESL_utilization   ///< Resource Utilization
};

/** @brief The attribute ids of RESL, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int RESL_Type[] = {
   RESL_value,
   RESL_id_instance,
   RESL_pe_task_id,
   RESL_amount,
   RESL_properties,
   RESL_utilization,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of RESL
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define RESL_ATTRIBUTES \
   {RESL_value, "RESL_value", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RESL_id_instance, "RESL_id_instance", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RESL_pe_task_id, "RESL_pe_task_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RESL_amount, "RESL_amount", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RESL_properties, "RESL_properties", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RESL_utilization, "RESL_utilization", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

