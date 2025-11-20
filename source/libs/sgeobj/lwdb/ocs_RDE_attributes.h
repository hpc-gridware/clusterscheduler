#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/RDE.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RDE_time = 10900,
   RDE_amount,
   RDE_resource_map_list,
   RDE_binding_inuse
};

constexpr const int RDE_Type[] = {
   RDE_time,
   RDE_amount,
   RDE_resource_map_list,
   RDE_binding_inuse,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RDE_ATTRIBUTES \
   {RDE_time, "RDE_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RDE_amount, "RDE_amount", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RDE_resource_map_list, "RDE_resource_map_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {RDE_binding_inuse, "RDE_binding_inuse", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

