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
 * This code was generated from file source/libs/sgeobj/json/PERM.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Object used to request given permissions of user and host.
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of PERM
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   PERM_is_manager = 7150,   ///< true if manager
   PERM_is_operator,   ///< true if operator
   PERM_is_admin_host,   ///< true if admin host
   PERM_is_submit_host,   ///< true if submit host
   PERM_host,   ///< hostname
   PERM_username   ///< username
};

/** @brief The attribute ids of PERM, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int PERM_Type[] = {
   PERM_is_manager,
   PERM_is_operator,
   PERM_is_admin_host,
   PERM_is_submit_host,
   PERM_host,
   PERM_username,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of PERM
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define PERM_ATTRIBUTES \
   {PERM_is_manager, "PERM_is_manager", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PERM_is_operator, "PERM_is_operator", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PERM_is_admin_host, "PERM_is_admin_host", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PERM_is_submit_host, "PERM_is_submit_host", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PERM_host, "PERM_host", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PERM_username, "PERM_username", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

