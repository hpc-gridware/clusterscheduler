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
 * This code was generated from file source/libs/sgeobj/json/JRS.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JRS_scope = 200,
   JRS_hard_resource_list,
   JRS_soft_resource_list,
   JRS_hard_queue_list,
   JRS_soft_queue_list,
   JRS_allocation_rule,
   JRS_ignore_slave_requests_on_master_host
};

constexpr const int JRS_Type[] = {
   JRS_scope,
   JRS_hard_resource_list,
   JRS_soft_resource_list,
   JRS_hard_queue_list,
   JRS_soft_queue_list,
   JRS_allocation_rule,
   JRS_ignore_slave_requests_on_master_host,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JRS_ATTRIBUTES \
   {JRS_scope, "JRS_scope", AttributeStatic::UINT32, AttributeStatic::UNORDERED_NON_UNIQUE}, \
   {JRS_hard_resource_list, "JRS_hard_resource_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JRS_soft_resource_list, "JRS_soft_resource_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JRS_hard_queue_list, "JRS_hard_queue_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JRS_soft_queue_list, "JRS_soft_queue_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JRS_allocation_rule, "JRS_allocation_rule", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JRS_ignore_slave_requests_on_master_host, "JRS_ignore_slave_requests_on_master_host", AttributeStatic::BOOL, AttributeStatic::NO_HASH} \

} // end namespace

