#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/GRU.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   GRU_type = 4050,
   GRU_name,
   GRU_amount,
   GRU_resource_map_list,
   GRU_host
};

constexpr const int GRU_Type[] = {
   GRU_type,
   GRU_name,
   GRU_amount,
   GRU_resource_map_list,
   GRU_host,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define GRU_ATTRIBUTES \
   {GRU_type, "GRU_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {GRU_name, "GRU_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {GRU_amount, "GRU_amount", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {GRU_resource_map_list, "GRU_resource_map_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {GRU_host, "GRU_host", AttributeStatic::HOST, AttributeStatic::NO_HASH} \

} // end namespace

