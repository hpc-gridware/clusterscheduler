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
 * This code was generated from file source/libs/sgeobj/json/LDR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   LDR_queue_ref_list = 11200,
   LDR_limit,
   LDR_global,
   LDR_host,
   LDR_queue
};

constexpr const int LDR_Type[] = {
   LDR_queue_ref_list,
   LDR_limit,
   LDR_global,
   LDR_host,
   LDR_queue,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define LDR_ATTRIBUTES \
   {LDR_queue_ref_list, "LDR_queue_ref_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LDR_limit, "LDR_limit", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LDR_global, "LDR_global", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LDR_host, "LDR_host", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LDR_queue, "LDR_queue", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

