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
 * This code was generated from file source/libs/sgeobj/json/AR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(AR_id) - @todo add summary
*    @todo add description
*
*    SGE_STRING(AR_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(AR_account) - @todo add summary
*    @todo add description
*
*    SGE_STRING(AR_owner) - @todo add summary
*    @todo add description
*
*    SGE_STRING(AR_group) - @todo add summary
*    @todo add description
*
*    SGE_ULONG64(AR_submission_time) - @todo add summary
*    ... microseconds since epoch
*
*    SGE_ULONG64(AR_start_time) - @todo add summary
*    ... microseconds since epoch
*
*    SGE_ULONG64(AR_end_time) - @todo add summary
*    ... microseconds since epoch
*
*    SGE_ULONG64(AR_duration) - @todo add summary
*    ... microseconds
*
*    SGE_ULONG(AR_verify) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(AR_error_handling) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(AR_state) - @todo add summary
*    @todo add description
*
*    SGE_STRING(AR_checkpoint_name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_resource_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_resource_utilization) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_queue_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_granted_slots) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_reserved_queues) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(AR_mail_options) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_mail_list) - @todo add summary
*    @todo add description
*
*    SGE_STRING(AR_pe) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_pe_range) - @todo add summary
*    @todo add description
*
*    SGE_STRING(AR_granted_pe) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_master_queue_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_acl_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(AR_xacl_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(AR_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(AR_qi_errors) - @todo add summary
*    @todo add description
*
*/

enum {
   AR_id = AR_LOWERBOUND,
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
   AR_qi_errors
};

LISTDEF(AR_Type)
   SGE_ULONG(AR_id, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_STRING(AR_name, CULL_SPOOL)
   SGE_STRING(AR_account, CULL_SPOOL)
   SGE_STRING(AR_owner, CULL_SPOOL)
   SGE_STRING(AR_group, CULL_SPOOL)
   SGE_ULONG64(AR_submission_time, CULL_SPOOL)
   SGE_ULONG64(AR_start_time, CULL_SPOOL)
   SGE_ULONG64(AR_end_time, CULL_SPOOL)
   SGE_ULONG64(AR_duration, CULL_SPOOL)
   SGE_ULONG(AR_verify, CULL_SPOOL)
   SGE_ULONG(AR_error_handling, CULL_SPOOL)
   SGE_ULONG(AR_state, CULL_SPOOL)
   SGE_STRING(AR_checkpoint_name, CULL_SPOOL)
   SGE_LIST(AR_resource_list, CE_Type, CULL_SPOOL)
   SGE_LIST(AR_resource_utilization, RUE_Type, CULL_DEFAULT)
   SGE_LIST(AR_queue_list, QR_Type, CULL_SPOOL)
   SGE_LIST(AR_granted_slots, JG_Type, CULL_SPOOL)
   SGE_LIST(AR_reserved_queues, QU_Type, CULL_SPOOL)
   SGE_ULONG(AR_mail_options, CULL_SPOOL)
   SGE_LIST(AR_mail_list, MR_Type, CULL_SPOOL)
   SGE_STRING(AR_pe, CULL_SPOOL)
   SGE_LIST(AR_pe_range, RN_Type, CULL_SPOOL)
   SGE_STRING(AR_granted_pe, CULL_SPOOL)
   SGE_LIST(AR_master_queue_list, QR_Type, CULL_SPOOL)
   SGE_LIST(AR_acl_list, ARA_Type, CULL_SPOOL)
   SGE_LIST(AR_xacl_list, ARA_Type, CULL_SPOOL)
   SGE_ULONG(AR_type, CULL_SPOOL)
   SGE_ULONG(AR_qi_errors, CULL_DEFAULT)
LISTEND

NAMEDEF(ARN)
   NAME("AR_id")
   NAME("AR_name")
   NAME("AR_account")
   NAME("AR_owner")
   NAME("AR_group")
   NAME("AR_submission_time")
   NAME("AR_start_time")
   NAME("AR_end_time")
   NAME("AR_duration")
   NAME("AR_verify")
   NAME("AR_error_handling")
   NAME("AR_state")
   NAME("AR_checkpoint_name")
   NAME("AR_resource_list")
   NAME("AR_resource_utilization")
   NAME("AR_queue_list")
   NAME("AR_granted_slots")
   NAME("AR_reserved_queues")
   NAME("AR_mail_options")
   NAME("AR_mail_list")
   NAME("AR_pe")
   NAME("AR_pe_range")
   NAME("AR_granted_pe")
   NAME("AR_master_queue_list")
   NAME("AR_acl_list")
   NAME("AR_xacl_list")
   NAME("AR_type")
   NAME("AR_qi_errors")
NAMEEND

#define AR_SIZE sizeof(ARN)/sizeof(char *)


