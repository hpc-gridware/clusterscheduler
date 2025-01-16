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
 * This code was generated from file source/libs/sgeobj/json/EVS.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   EVS_id = 1250,
   EVS_flush,
   EVS_interval,
   EVS_what,
   EVS_where
};

constexpr const int EVS_Type[] = {
   EVS_id,
   EVS_flush,
   EVS_interval,
   EVS_what,
   EVS_where,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define EVS_ATTRIBUTES \
   {EVS_id, "EVS_id", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EVS_flush, "EVS_flush", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {EVS_interval, "EVS_interval", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EVS_what, "EVS_what", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {EVS_where, "EVS_where", AttributeStatic::OBJECT, AttributeStatic::NO_HASH} \

} // end namespace

