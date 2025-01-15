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
 * This code was generated from file source/libs/sgeobj/json/RQL.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RQL_name = 12300,
   RQL_result,
   RQL_time,
   RQL_slots,
   RQL_slots_qend,
   RQL_tagged4schedule
};

constexpr const int RQL_Type[] = {
   RQL_name,
   RQL_result,
   RQL_time,
   RQL_slots,
   RQL_slots_qend,
   RQL_tagged4schedule,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RQL_ATTRIBUTES \
   {RQL_name, "RQL_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {RQL_result, "RQL_result", AttributeStatic::INT, AttributeStatic::NO_HASH}, \
   {RQL_time, "RQL_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {RQL_slots, "RQL_slots", AttributeStatic::INT, AttributeStatic::NO_HASH}, \
   {RQL_slots_qend, "RQL_slots_qend", AttributeStatic::INT, AttributeStatic::NO_HASH}, \
   {RQL_tagged4schedule, "RQL_tagged4schedule", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

