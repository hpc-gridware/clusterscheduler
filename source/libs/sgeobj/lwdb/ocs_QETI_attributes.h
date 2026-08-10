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
 * This code was generated from file source/libs/sgeobj/json/QETI.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue End Time Iterator
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of QETI
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   QETI_total = 10700,   ///< Total Capacity
   QETI_resource_instance,   ///< Resource Instance
   QETI_queue_end_next   ///< Next Position
};

/** @brief The attribute ids of QETI, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int QETI_Type[] = {
   QETI_total,
   QETI_resource_instance,
   QETI_queue_end_next,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of QETI
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define QETI_ATTRIBUTES \
   {QETI_total, "QETI_total", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QETI_resource_instance, "QETI_resource_instance", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QETI_queue_end_next, "QETI_queue_end_next", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

