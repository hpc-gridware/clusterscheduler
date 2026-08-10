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
 * This code was generated from file source/libs/sgeobj/json/CTI.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Category Ignore Entry
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of CTI
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   CTI_name = 9900   ///< Name
};

/** @brief The attribute ids of CTI, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int CTI_Type[] = {
   CTI_name,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of CTI
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define CTI_ATTRIBUTES \
   {CTI_name, "CTI_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, false} \

} // end namespace

