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
 * This code was generated from file source/libs/sgeobj/json/PARA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Parameter
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of PARA
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   PARA_name = 10000,   ///< Name
   PARA_value   ///< Value
};

/** @brief The attribute ids of PARA, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int PARA_Type[] = {
   PARA_name,
   PARA_value,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of PARA
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define PARA_ATTRIBUTES \
   {PARA_name, "PARA_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, true, false}, \
   {PARA_value, "PARA_value", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

