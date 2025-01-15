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
 * This code was generated from file source/libs/sgeobj/json/HL.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   HL_name = 850,
   HL_value,
   HL_last_update,
   HL_is_static
};

constexpr const int HL_Type[] = {
   HL_name,
   HL_value,
   HL_last_update,
   HL_is_static,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define HL_ATTRIBUTES \
   {HL_name, "HL_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {HL_value, "HL_value", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {HL_last_update, "HL_last_update", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {HL_is_static, "HL_is_static", AttributeStatic::BOOL, AttributeStatic::NO_HASH} \

} // end namespace

