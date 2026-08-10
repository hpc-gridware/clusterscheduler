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
 * This code was generated from file source/libs/sgeobj/json/RQS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief @todo add summary
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of RQS
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   RQS_name = 11400,   ///< @todo add summary
   RQS_description,   ///< @todo add summary
   RQS_enabled,   ///< @todo add summary
   RQS_rule,   ///< @todo add summary
   RQS_joker   ///< Joker
};

/** @brief The attribute ids of RQS, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int RQS_Type[] = {
   RQS_name,
   RQS_description,
   RQS_enabled,
   RQS_rule,
   RQS_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of RQS
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define RQS_ATTRIBUTES \
   {RQS_name, "RQS_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {RQS_description, "RQS_description", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQS_enabled, "RQS_enabled", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQS_rule, "RQS_rule", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQS_joker, "RQS_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

