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
 * This code was generated from file source/libs/sgeobj/json/RQRL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Rule Limit
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of RQRL
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   RQRL_name = 11700,   ///< Resource Name
   RQRL_value,   ///< Configured Value
   RQRL_type,   ///< Value Type
   RQRL_dvalue,   ///< Evaluated Value
   RQRL_usage,   ///< Usage
   RQRL_dynamic   ///< Dynamic
};

/** @brief The attribute ids of RQRL, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int RQRL_Type[] = {
   RQRL_name,
   RQRL_value,
   RQRL_type,
   RQRL_dvalue,
   RQRL_usage,
   RQRL_dynamic,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of RQRL
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define RQRL_ATTRIBUTES \
   {RQRL_name, "RQRL_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {RQRL_value, "RQRL_value", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQRL_type, "RQRL_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQRL_dvalue, "RQRL_dvalue", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQRL_usage, "RQRL_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RQRL_dynamic, "RQRL_dynamic", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

