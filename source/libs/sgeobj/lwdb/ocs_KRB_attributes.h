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
 * This code was generated from file source/libs/sgeobj/json/KRB.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Kerberos Authentication
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of KRB
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   KRB_commproc = 5050,   ///< Commproc Id
   KRB_id,   ///< Id
   KRB_host,   ///< Host
   KRB_timestamp,   ///< Timestamp
   KRB_auth_context,   ///< Authentication Context
   KRB_tgt_list   ///< TGT List
};

/** @brief The attribute ids of KRB, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int KRB_Type[] = {
   KRB_commproc,
   KRB_id,
   KRB_host,
   KRB_timestamp,
   KRB_auth_context,
   KRB_tgt_list,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of KRB
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define KRB_ATTRIBUTES \
   {KRB_commproc, "KRB_commproc", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {KRB_id, "KRB_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {KRB_host, "KRB_host", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {KRB_timestamp, "KRB_timestamp", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {KRB_auth_context, "KRB_auth_context", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {KRB_tgt_list, "KRB_tgt_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

