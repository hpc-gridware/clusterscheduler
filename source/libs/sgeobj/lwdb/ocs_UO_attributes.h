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
 * This code was generated from file source/libs/sgeobj/json/UO.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   UO_name = 2450,
   UO_joker
};

constexpr const int UO_Type[] = {
   UO_name,
   UO_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define UO_ATTRIBUTES \
   {UO_name, "UO_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {UO_joker, "UO_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

