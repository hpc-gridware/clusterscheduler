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
 * This code was generated from file source/libs/sgeobj/json/PE.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   PE_name = 2550,
   PE_slots,
   PE_user_list,
   PE_xuser_list,
   PE_start_proc_args,
   PE_stop_proc_args,
   PE_allocation_rule,
   PE_control_slaves,
   PE_job_is_first_task,
   PE_resource_utilization,
   PE_urgency_slots,
   PE_accounting_summary,
   PE_master_forks_slaves,
   PE_daemon_forks_slaves,
   PE_ignore_slave_requests_on_master_host,
   PE_joker
};

constexpr const int PE_Type[] = {
   PE_name,
   PE_slots,
   PE_user_list,
   PE_xuser_list,
   PE_start_proc_args,
   PE_stop_proc_args,
   PE_allocation_rule,
   PE_control_slaves,
   PE_job_is_first_task,
   PE_resource_utilization,
   PE_urgency_slots,
   PE_accounting_summary,
   PE_master_forks_slaves,
   PE_daemon_forks_slaves,
   PE_ignore_slave_requests_on_master_host,
   PE_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define PE_ATTRIBUTES \
   {PE_name, "PE_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {PE_slots, "PE_slots", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {PE_user_list, "PE_user_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {PE_xuser_list, "PE_xuser_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {PE_start_proc_args, "PE_start_proc_args", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PE_stop_proc_args, "PE_stop_proc_args", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PE_allocation_rule, "PE_allocation_rule", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PE_control_slaves, "PE_control_slaves", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PE_job_is_first_task, "PE_job_is_first_task", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PE_resource_utilization, "PE_resource_utilization", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {PE_urgency_slots, "PE_urgency_slots", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PE_accounting_summary, "PE_accounting_summary", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PE_master_forks_slaves, "PE_master_forks_slaves", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PE_daemon_forks_slaves, "PE_daemon_forks_slaves", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PE_ignore_slave_requests_on_master_host, "PE_ignore_slave_requests_on_master_host", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {PE_joker, "PE_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

