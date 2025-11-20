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
 * This code was generated from file source/libs/sgeobj/json/BN.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   BN_new_type = 13100,
   BN_instance,
   BN_amount,
   BN_unit,
   BN_filter,
   BN_sort,
   BN_start,
   BN_stop,
   BN_strategy,
   BN_specific_hostname,
   BN_specific_binding,
   BN_specific_binding_list,
   BN_joker
};

constexpr const int BN_Type[] = {
   BN_new_type,
   BN_instance,
   BN_amount,
   BN_unit,
   BN_filter,
   BN_sort,
   BN_start,
   BN_stop,
   BN_strategy,
   BN_specific_hostname,
   BN_specific_binding,
   BN_specific_binding_list,
   BN_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define BN_ATTRIBUTES \
   {BN_new_type, "BN_new_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_instance, "BN_instance", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_amount, "BN_amount", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_unit, "BN_unit", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_filter, "BN_filter", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_sort, "BN_sort", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_start, "BN_start", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_stop, "BN_stop", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_strategy, "BN_strategy", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_specific_hostname, "BN_specific_hostname", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_specific_binding, "BN_specific_binding", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {BN_specific_binding_list, "BN_specific_binding_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {BN_joker, "BN_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

