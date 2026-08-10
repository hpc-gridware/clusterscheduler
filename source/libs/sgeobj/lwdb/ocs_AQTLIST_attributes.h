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
 * This code was generated from file source/libs/sgeobj/json/AQTLIST.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute Queue Type List
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of AQTLIST
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   AQTLIST_href = 9450,   ///< Host Reference
   AQTLIST_value   ///< Value
};

/** @brief The attribute ids of AQTLIST, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int AQTLIST_Type[] = {
   AQTLIST_href,
   AQTLIST_value,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of AQTLIST
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define AQTLIST_ATTRIBUTES \
   {AQTLIST_href, "AQTLIST_href", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {AQTLIST_value, "AQTLIST_value", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

