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
 * This code was generated from file source/libs/sgeobj/json/SO.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   SO_name = 3350,
   SO_threshold,
   SO_slots_sum,
   SO_seq_no,
   SO_action
};

constexpr const int SO_Type[] = {
   SO_name,
   SO_threshold,
   SO_slots_sum,
   SO_seq_no,
   SO_action,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define SO_ATTRIBUTES \
   {SO_name, "SO_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SO_threshold, "SO_threshold", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SO_slots_sum, "SO_slots_sum", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SO_seq_no, "SO_seq_no", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SO_action, "SO_action", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

