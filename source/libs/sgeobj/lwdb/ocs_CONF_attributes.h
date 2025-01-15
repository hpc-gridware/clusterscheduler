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
 * This code was generated from file source/libs/sgeobj/json/CONF.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   CONF_name = 2850,
   CONF_version,
   CONF_entries
};

constexpr const int CONF_Type[] = {
   CONF_name,
   CONF_version,
   CONF_entries,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define CONF_ATTRIBUTES \
   {CONF_name, "CONF_name", AttributeStatic::HOST, AttributeStatic::UNORDERED_UNIQUE}, \
   {CONF_version, "CONF_version", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CONF_entries, "CONF_entries", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

