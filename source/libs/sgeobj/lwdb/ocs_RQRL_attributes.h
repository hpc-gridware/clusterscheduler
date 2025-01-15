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
 * This code was generated from file source/libs/sgeobj/json/RQRL.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RQRL_name = 12200,
   RQRL_value,
   RQRL_type,
   RQRL_dvalue,
   RQRL_usage,
   RQRL_dynamic
};

constexpr const int RQRL_Type[] = {
   RQRL_name,
   RQRL_value,
   RQRL_type,
   RQRL_dvalue,
   RQRL_usage,
   RQRL_dynamic,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RQRL_ATTRIBUTES \
   {RQRL_name, "RQRL_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {RQRL_value, "RQRL_value", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {RQRL_type, "RQRL_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {RQRL_dvalue, "RQRL_dvalue", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {RQRL_usage, "RQRL_usage", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {RQRL_dynamic, "RQRL_dynamic", AttributeStatic::BOOL, AttributeStatic::NO_HASH} \

} // end namespace

