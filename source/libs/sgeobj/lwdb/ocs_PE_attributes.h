#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/** @file
 * @brief Parallel Environment
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of PE
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   PE_name = 2150,   ///< Name
   PE_slots,   ///< Total Slots
   PE_user_list,   ///< User List
   PE_xuser_list,   ///< XUser List
   PE_start_proc_args,   ///< Start Procedure
   PE_stop_proc_args,   ///< Stop Procedure
   PE_allocation_rule,   ///< Allocation Rule
   PE_control_slaves,   ///< Control Slaves
   PE_job_is_first_task,   ///< Job Is First Task
   PE_resource_utilization,   ///< Resource Utilization
   PE_urgency_slots,   ///< Urgency Slots
   PE_accounting_summary,   ///< Accounting Summary
   PE_master_forks_slaves,   ///< Master Forks Slaves
   PE_daemon_forks_slaves,   ///< Daemon Forks Slaves
   PE_ignore_slave_requests_on_master_host,   ///< Daemon Forks Slaves
   PE_joker   ///< Joker
};

/** @brief The attribute ids of PE, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
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

/** @brief The compile-time description of every attribute of PE
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define PE_ATTRIBUTES \
   {PE_name, "PE_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {PE_slots, "PE_slots", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_user_list, "PE_user_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_xuser_list, "PE_xuser_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_start_proc_args, "PE_start_proc_args", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_stop_proc_args, "PE_stop_proc_args", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_allocation_rule, "PE_allocation_rule", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_control_slaves, "PE_control_slaves", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_job_is_first_task, "PE_job_is_first_task", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_resource_utilization, "PE_resource_utilization", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PE_urgency_slots, "PE_urgency_slots", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_accounting_summary, "PE_accounting_summary", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_master_forks_slaves, "PE_master_forks_slaves", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_daemon_forks_slaves, "PE_daemon_forks_slaves", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_ignore_slave_requests_on_master_host, "PE_ignore_slave_requests_on_master_host", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {PE_joker, "PE_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

