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
 * This code was generated from file source/libs/sgeobj/json/RQR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RQR_name = 12000,
   RQR_filter_users,
   RQR_filter_projects,
   RQR_filter_pes,
   RQR_filter_queues,
   RQR_filter_hosts,
   RQR_limit,
   RQR_level
};

constexpr const int RQR_Type[] = {
   RQR_name,
   RQR_filter_users,
   RQR_filter_projects,
   RQR_filter_pes,
   RQR_filter_queues,
   RQR_filter_hosts,
   RQR_limit,
   RQR_level,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RQR_ATTRIBUTES \
   {RQR_name, "RQR_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {RQR_filter_users, "RQR_filter_users", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {RQR_filter_projects, "RQR_filter_projects", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {RQR_filter_pes, "RQR_filter_pes", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {RQR_filter_queues, "RQR_filter_queues", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {RQR_filter_hosts, "RQR_filter_hosts", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {RQR_limit, "RQR_limit", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {RQR_level, "RQR_level", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

