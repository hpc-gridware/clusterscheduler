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
 * This code was generated from file source/libs/sgeobj/json/BN.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   BN_strategy = 13200,
   BN_type,
   BN_parameter_n,
   BN_parameter_socket_offset,
   BN_parameter_core_offset,
   BN_parameter_striding_step_size,
   BN_parameter_explicit,
   BN_joker
};

constexpr const int BN_Type[] = {
   BN_strategy,
   BN_type,
   BN_parameter_n,
   BN_parameter_socket_offset,
   BN_parameter_core_offset,
   BN_parameter_striding_step_size,
   BN_parameter_explicit,
   BN_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define BN_ATTRIBUTES \
   {BN_strategy, "BN_strategy", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {BN_type, "BN_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {BN_parameter_n, "BN_parameter_n", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {BN_parameter_socket_offset, "BN_parameter_socket_offset", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {BN_parameter_core_offset, "BN_parameter_core_offset", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {BN_parameter_striding_step_size, "BN_parameter_striding_step_size", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {BN_parameter_explicit, "BN_parameter_explicit", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {BN_joker, "BN_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

