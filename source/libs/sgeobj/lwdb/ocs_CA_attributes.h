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
 * This code was generated from file source/libs/sgeobj/json/CA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Parsed Calendar
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of CA
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   CA_yday_range_list = 5750,   ///< YearDay Range List
   CA_wday_range_list,   ///< WeekDay Range List
   CA_daytime_range_list,   ///< DayTime Range List
   CA_state   ///< State
};

/** @brief The attribute ids of CA, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int CA_Type[] = {
   CA_yday_range_list,
   CA_wday_range_list,
   CA_daytime_range_list,
   CA_state,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of CA
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define CA_ATTRIBUTES \
   {CA_yday_range_list, "CA_yday_range_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CA_wday_range_list, "CA_wday_range_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CA_daytime_range_list, "CA_daytime_range_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CA_state, "CA_state", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

