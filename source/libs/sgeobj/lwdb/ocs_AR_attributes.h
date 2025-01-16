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
 * This code was generated from file source/libs/sgeobj/json/AR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   AR_id = 12400,
   AR_name,
   AR_account,
   AR_owner,
   AR_group,
   AR_submission_time,
   AR_start_time,
   AR_end_time,
   AR_duration,
   AR_verify,
   AR_error_handling,
   AR_state,
   AR_checkpoint_name,
   AR_resource_list,
   AR_resource_utilization,
   AR_queue_list,
   AR_granted_slots,
   AR_reserved_hosts,
   AR_reserved_queues,
   AR_mail_options,
   AR_mail_list,
   AR_pe,
   AR_pe_range,
   AR_granted_pe,
   AR_master_queue_list,
   AR_acl_list,
   AR_xacl_list,
   AR_type,
   AR_qi_errors,
   AR_request_set_list,
   AR_joker
};

constexpr const int AR_Type[] = {
   AR_id,
   AR_name,
   AR_account,
   AR_owner,
   AR_group,
   AR_submission_time,
   AR_start_time,
   AR_end_time,
   AR_duration,
   AR_verify,
   AR_error_handling,
   AR_state,
   AR_checkpoint_name,
   AR_resource_list,
   AR_resource_utilization,
   AR_queue_list,
   AR_granted_slots,
   AR_reserved_hosts,
   AR_reserved_queues,
   AR_mail_options,
   AR_mail_list,
   AR_pe,
   AR_pe_range,
   AR_granted_pe,
   AR_master_queue_list,
   AR_acl_list,
   AR_xacl_list,
   AR_type,
   AR_qi_errors,
   AR_request_set_list,
   AR_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define AR_ATTRIBUTES \
   {AR_id, "AR_id", AttributeStatic::UINT32, AttributeStatic::UNORDERED_UNIQUE}, \
   {AR_name, "AR_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {AR_account, "AR_account", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {AR_owner, "AR_owner", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {AR_group, "AR_group", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {AR_submission_time, "AR_submission_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {AR_start_time, "AR_start_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {AR_end_time, "AR_end_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {AR_duration, "AR_duration", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {AR_verify, "AR_verify", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {AR_error_handling, "AR_error_handling", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {AR_state, "AR_state", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {AR_checkpoint_name, "AR_checkpoint_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {AR_resource_list, "AR_resource_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_resource_utilization, "AR_resource_utilization", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_queue_list, "AR_queue_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_granted_slots, "AR_granted_slots", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_reserved_hosts, "AR_reserved_hosts", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_reserved_queues, "AR_reserved_queues", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_mail_options, "AR_mail_options", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {AR_mail_list, "AR_mail_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_pe, "AR_pe", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {AR_pe_range, "AR_pe_range", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_granted_pe, "AR_granted_pe", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {AR_master_queue_list, "AR_master_queue_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_acl_list, "AR_acl_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_xacl_list, "AR_xacl_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_type, "AR_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {AR_qi_errors, "AR_qi_errors", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {AR_request_set_list, "AR_request_set_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {AR_joker, "AR_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

