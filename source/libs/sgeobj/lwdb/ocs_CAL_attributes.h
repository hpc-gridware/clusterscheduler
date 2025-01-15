#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   CAL_name = 6050,
   CAL_year_calendar,
   CAL_week_calendar,
   CAL_parsed_year_calendar,
   CAL_parsed_week_calendar,
   CAL_joker
};

constexpr const int CAL_Type[] = {
   CAL_name,
   CAL_year_calendar,
   CAL_week_calendar,
   CAL_parsed_year_calendar,
   CAL_parsed_week_calendar,
   CAL_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define CAL_ATTRIBUTES \
   {CAL_name, "CAL_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {CAL_year_calendar, "CAL_year_calendar", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CAL_week_calendar, "CAL_week_calendar", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CAL_parsed_year_calendar, "CAL_parsed_year_calendar", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CAL_parsed_week_calendar, "CAL_parsed_week_calendar", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CAL_joker, "CAL_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

