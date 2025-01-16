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
 * This code was generated from file source/libs/sgeobj/json/PERM.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   PERM_is_manager = 7550,
   PERM_is_operator,
   PERM_is_admin_host,
   PERM_is_submit_host,
   PERM_host,
   PERM_username
};

constexpr const int PERM_Type[] = {
   PERM_is_manager,
   PERM_is_operator,
   PERM_is_admin_host,
   PERM_is_submit_host,
   PERM_host,
   PERM_username,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define PERM_ATTRIBUTES \
   {PERM_is_manager, "PERM_is_manager", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PERM_is_operator, "PERM_is_operator", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PERM_is_admin_host, "PERM_is_admin_host", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PERM_is_submit_host, "PERM_is_submit_host", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PERM_host, "PERM_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {PERM_username, "PERM_username", AttributeStatic::STRING, AttributeStatic::NO_HASH} \

} // end namespace

