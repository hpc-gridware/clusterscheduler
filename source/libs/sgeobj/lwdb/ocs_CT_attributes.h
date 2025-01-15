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
 * This code was generated from file source/libs/sgeobj/json/CT.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   CT_str = 7050,
   CT_refcount,
   CT_count,
   CT_rejected,
   CT_cache,
   CT_messages_added,
   CT_resource_contribution,
   CT_rc_valid,
   CT_reservation_rejected
};

constexpr const int CT_Type[] = {
   CT_str,
   CT_refcount,
   CT_count,
   CT_rejected,
   CT_cache,
   CT_messages_added,
   CT_resource_contribution,
   CT_rc_valid,
   CT_reservation_rejected,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define CT_ATTRIBUTES \
   {CT_str, "CT_str", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {CT_refcount, "CT_refcount", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CT_count, "CT_count", AttributeStatic::INT, AttributeStatic::NO_HASH}, \
   {CT_rejected, "CT_rejected", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CT_cache, "CT_cache", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CT_messages_added, "CT_messages_added", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {CT_resource_contribution, "CT_resource_contribution", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {CT_rc_valid, "CT_rc_valid", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {CT_reservation_rejected, "CT_reservation_rejected", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

