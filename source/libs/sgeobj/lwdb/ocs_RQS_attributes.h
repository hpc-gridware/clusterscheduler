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
 * This code was generated from file source/libs/sgeobj/json/RQS.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RQS_name = 11900,
   RQS_description,
   RQS_enabled,
   RQS_rule,
   RQS_joker
};

constexpr const int RQS_Type[] = {
   RQS_name,
   RQS_description,
   RQS_enabled,
   RQS_rule,
   RQS_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RQS_ATTRIBUTES \
   {RQS_name, "RQS_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {RQS_description, "RQS_description", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {RQS_enabled, "RQS_enabled", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {RQS_rule, "RQS_rule", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {RQS_joker, "RQS_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

