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
 * This code was generated from file source/libs/sgeobj/json/AQTLIST.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   AQTLIST_href = 9950,
   AQTLIST_value
};

constexpr const int AQTLIST_Type[] = {
   AQTLIST_href,
   AQTLIST_value,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define AQTLIST_ATTRIBUTES \
   {AQTLIST_href, "AQTLIST_href", AttributeStatic::HOST, AttributeStatic::UNORDERED_UNIQUE}, \
   {AQTLIST_value, "AQTLIST_value", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

