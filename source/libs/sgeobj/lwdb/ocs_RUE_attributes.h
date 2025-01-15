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
 * This code was generated from file source/libs/sgeobj/json/RUE.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RUE_name = 11100,
   RUE_utilized_now,
   RUE_utilized_now_resource_map_list,
   RUE_utilized,
   RUE_utilized_now_nonexclusive,
   RUE_utilized_nonexclusive
};

constexpr const int RUE_Type[] = {
   RUE_name,
   RUE_utilized_now,
   RUE_utilized_now_resource_map_list,
   RUE_utilized,
   RUE_utilized_now_nonexclusive,
   RUE_utilized_nonexclusive,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RUE_ATTRIBUTES \
   {RUE_name, "RUE_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {RUE_utilized_now, "RUE_utilized_now", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {RUE_utilized_now_resource_map_list, "RUE_utilized_now_resource_map_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {RUE_utilized, "RUE_utilized", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {RUE_utilized_now_nonexclusive, "RUE_utilized_now_nonexclusive", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {RUE_utilized_nonexclusive, "RUE_utilized_nonexclusive", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

