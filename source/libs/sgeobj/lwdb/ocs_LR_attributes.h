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
 * This code was generated from file source/libs/sgeobj/json/LR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   LR_name = 1450,
   LR_value,
   LR_global,
   LR_is_static,
   LR_host
};

constexpr const int LR_Type[] = {
   LR_name,
   LR_value,
   LR_global,
   LR_is_static,
   LR_host,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define LR_ATTRIBUTES \
   {LR_name, "LR_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {LR_value, "LR_value", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {LR_global, "LR_global", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {LR_is_static, "LR_is_static", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {LR_host, "LR_host", AttributeStatic::HOST, AttributeStatic::UNORDERED_UNIQUE} \

} // end namespace

