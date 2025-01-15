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
 * This code was generated from file source/libs/sgeobj/json/KRB.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   KRB_commproc = 5450,
   KRB_id,
   KRB_host,
   KRB_timestamp,
   KRB_auth_context,
   KRB_tgt_list
};

constexpr const int KRB_Type[] = {
   KRB_commproc,
   KRB_id,
   KRB_host,
   KRB_timestamp,
   KRB_auth_context,
   KRB_tgt_list,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define KRB_ATTRIBUTES \
   {KRB_commproc, "KRB_commproc", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {KRB_id, "KRB_id", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {KRB_host, "KRB_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {KRB_timestamp, "KRB_timestamp", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {KRB_auth_context, "KRB_auth_context", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {KRB_tgt_list, "KRB_tgt_list", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

