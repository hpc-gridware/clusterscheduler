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
 * This code was generated from file source/libs/sgeobj/json/CAL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Calendar
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of CAL
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   CAL_name = 5650,   ///< Name
   CAL_year_calendar,   ///< Year Calendar
   CAL_week_calendar,   ///< Week Calendar
   CAL_parsed_year_calendar,   ///< Parsed Year Calendar
   CAL_parsed_week_calendar,   ///< Parsed Week Calendar
   CAL_joker   ///< Joker
};

/** @brief The attribute ids of CAL, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int CAL_Type[] = {
   CAL_name,
   CAL_year_calendar,
   CAL_week_calendar,
   CAL_parsed_year_calendar,
   CAL_parsed_week_calendar,
   CAL_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of CAL
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define CAL_ATTRIBUTES \
   {CAL_name, "CAL_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {CAL_year_calendar, "CAL_year_calendar", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CAL_week_calendar, "CAL_week_calendar", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CAL_parsed_year_calendar, "CAL_parsed_year_calendar", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CAL_parsed_week_calendar, "CAL_parsed_week_calendar", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CAL_joker, "CAL_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

