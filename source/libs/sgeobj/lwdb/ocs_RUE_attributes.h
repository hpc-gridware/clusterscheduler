#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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
   RUE_name = 11000,
   RUE_utilized_now,
   RUE_utilized_now_resource_map_list,
   RUE_utilized_now_binding_inuse,
   RUE_utilized,
   RUE_utilized_now_nonexclusive,
   RUE_utilized_nonexclusive
};

constexpr const int RUE_Type[] = {
   RUE_name,
   RUE_utilized_now,
   RUE_utilized_now_resource_map_list,
   RUE_utilized_now_binding_inuse,
   RUE_utilized,
   RUE_utilized_now_nonexclusive,
   RUE_utilized_nonexclusive,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RUE_ATTRIBUTES \
   {RUE_name, "RUE_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {RUE_utilized_now, "RUE_utilized_now", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized_now_resource_map_list, "RUE_utilized_now_resource_map_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized_now_binding_inuse, "RUE_utilized_now_binding_inuse", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized, "RUE_utilized", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized_now_nonexclusive, "RUE_utilized_now_nonexclusive", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RUE_utilized_nonexclusive, "RUE_utilized_nonexclusive", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

