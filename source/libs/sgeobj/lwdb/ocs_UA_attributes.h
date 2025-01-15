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
 * This code was generated from file source/libs/sgeobj/json/UA.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   UA_name = 3750,
   UA_value
};

constexpr const int UA_Type[] = {
   UA_name,
   UA_value,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define UA_ATTRIBUTES \
   {UA_name, "UA_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {UA_value, "UA_value", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH} \

} // end namespace

