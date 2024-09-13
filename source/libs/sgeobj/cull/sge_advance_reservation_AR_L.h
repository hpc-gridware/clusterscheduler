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
* @brief Advance Reservation
*
* An object of this type represents an advance reservation.
* An advance reservation reserves (consumable) resources for a certain time period.
* Jobs which are submitted into the AR can use these resources.
* See also man pages qrsub.1, qrstat.1, and qrdel.1
*
*    SGE_ULONG(AR_id) - AR Id
*    A unique identifier for the advance reservation, similar to the job id.
*
*    SGE_STRING(AR_name) - AR Name
*    A name for the advance reservation.
*
*    SGE_STRING(AR_account) - Account
*    The account to which the advance reservation is charged.
*
*    SGE_STRING(AR_owner) - Owner
*    The owner of the advance reservation (the user who submitted the AR).
*
*    SGE_STRING(AR_group) - Group
*    The UNIX group of the advance reservation owner.
*
*    SGE_ULONG64(AR_submission_time) - Submission Time
*    The time when the AR was submitted in microseconds since epoch.
*
*    SGE_ULONG64(AR_start_time) - Start Time
*    The start time of the AR in microseconds since epoch
*
*    SGE_ULONG64(AR_end_time) - End Time
*    The end time of the AR in microseconds since epoch
*
*    SGE_ULONG64(AR_duration) - Duration
*    The duration of the AR in microseconds
*
*    SGE_ULONG(AR_verify) - Verify
*    From qrsub -w v|e.
*
*    SGE_ULONG(AR_error_handling) - Error Handling
*    From qrsub -he yes/no.
*
*    SGE_ULONG(AR_state) - State
*    The state of the advance reservation.
*
*    SGE_STRING(AR_checkpoint_name) - Checkpoint Name
*    Checkpointing environment jobs running in the AR can use.
*
*    SGE_LIST(AR_resource_list) - Resource List
*    The list of resources requested by the advance reservation (qrsub -l).
*    Just one hard resource list. @todo The -scope feature is still missing.
*
*    SGE_LIST(AR_resource_utilization) - Resource Utilization
*    The utilization of resources by jobs running in the AR.
*
*    SGE_LIST(AR_queue_list) - Queue List
*    The list of queues requested by the advance reservation (qrsub -q).
*    Just one hard queue list. @todo The -scope feature is still missing.
*
*    SGE_LIST(AR_granted_slots) - @todo add summary
*    The list of queues and the number of slots which are reserved for the advance reservation.
*    Equivalent to the JAT_granted_destin_identifier_list in running jobs/array tasks.
*
*    SGE_LIST(AR_reserved_hosts) - Reserved Hosts
*    Will hold the list of hosts which are reserved for the advance reservation
*    together with the amount of granted consumables in EH_consumable_resources.
*    @todo not yet implemented, see CS-430
*
*    SGE_LIST(AR_reserved_queues) - Reserved Queues
*    Holds the list of queues which are reserved for the advance reservation
*    together with the amount of granted consumables in QU_consumable_resources.
*
*    SGE_ULONG(AR_mail_options) - Mail Options
*    Mail options for the advance reservation from qrsub -m.
*
*    SGE_LIST(AR_mail_list) - Mail List
*    Mail list for the advance reservation from qrsub -M.
*
*    SGE_STRING(AR_pe) - Parallel Environment
*    The parallel environment which is requested by the advance reservation from qrsub -pe.
*
*    SGE_LIST(AR_pe_range) - PE Range
*    The number of slots (can be a range) which are requested for the advance reservation.
*
*    SGE_STRING(AR_granted_pe) - Granted PE
*    The parallel environment which was granted to the advance reservation based on the pe request.
*
*    SGE_LIST(AR_master_queue_list) - Master Queue List
*    The list of possible master queues requested by the advance reservation (qrsub -masterq).
*
*    SGE_LIST(AR_acl_list) - ACL List
*    The acl_list defines which users may use the advance reservation.
*
*    SGE_LIST(AR_xacl_list) - XACL List
*    The xacl_list defines which users may not use the advance reservation.
*
*    SGE_ULONG(AR_type) - Type
*    Holds the information from the qrsub -now option (immediate or batch).
*
*    SGE_ULONG(AR_qi_errors) - QI Errors
*    Number of queue instances which are in some error state.
*
*    SGE_LIST(AR_request_set_list) - Request Set List
*    @todo placeholder for implementing the -scope switch
*    List of request sets. 0 .. 3 request sets can exist: global, master, slave.
*    Sequential ARs can have a single job request set, the global one.
*    Parallel ARs can have up to 3 request sets: global requests,
*    requests for the master task, requests for the slave tasks.
*
*    SGE_LIST(AR_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
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
   SGE_LIST(AR_reserved_hosts, EH_Type, CULL_SPOOL)
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
   SGE_LIST(AR_request_set_list, JRS_Type, CULL_SPOOL)
   SGE_LIST(AR_joker, VA_Type, CULL_SPOOL)
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
   NAME("AR_reserved_hosts")
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
   NAME("AR_request_set_list")
   NAME("AR_joker")
NAMEEND

#define AR_SIZE sizeof(ARN)/sizeof(char *)


