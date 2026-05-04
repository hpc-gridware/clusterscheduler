#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/RL.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RL_name = 13500,
   RL_enabled,
   RL_user_list,
   RL_parent_role_list,
   RL_perm_list,
   RL_joker
};

constexpr const int RL_Type[] = {
   RL_name,
   RL_enabled,
   RL_user_list,
   RL_parent_role_list,
   RL_perm_list,
   RL_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RL_ATTRIBUTES \
   {RL_name, "RL_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {RL_enabled, "RL_enabled", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RL_user_list, "RL_user_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RL_parent_role_list, "RL_parent_role_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RL_perm_list, "RL_perm_list", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RL_joker, "RL_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

