#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/OR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Scheduler Order
*
* A single scheduler order, e.g. to start a job
*
*    SGE_ULONG(OR_type) - Order Type
*    The type of the order. Defined in an enum in libs/sgeobj/sge_order.h, e.g.
*      - ORT_start_job
*      - ORT_tickets
*      - ORT_ptickets
*      - ORT_remove_job
*      - ORT_...
*
*    SGE_ULONG(OR_job_number) - Job Number
*    The job id in case of job related orders.
*
*    SGE_ULONG(OR_ja_task_number) - Array Task Number
*    The array task id in case of array job related orders.
*
*    SGE_ULONG(OR_job_version) - Job Version
*    A job version number. Used to detect if a scheduler order is based on an older version of the job.
*    When a job is modified, the job version (in the job object) is increased.
*    If a scheduling decision has been done on an older version of the job the order is ignored.
*
*    SGE_LIST(OR_queuelist) - Queue Instances
*    List of queue instances a job has been scheduled to
*
*    SGE_LIST(OR_granted_resources_list) - Granted Resources List
*    List of RSMAP (future: and resource) requests.
*
*    SGE_DOUBLE(OR_ticket) - Number of Tickets
*    Number of tickets a job got during scheduling.
*
*    SGE_LIST(OR_joker) - Order Specific Data
*    Sublist with order specific data, depending on the order type:
*      - ORT_start_job:              empty
*      - ORT_remove_job:             empty
*      - ORT_tickets:                reduced job element
*      - ORT_update_*_usage:         reduced user or project object
*      - ORT_share_tree:             reduced share tree root node
*      - ORT_remove_immediate_job:   empty
*      - ORT_job_schedd_info:        scheduler messages (SME_Type)
*      - ORT_ptickets:               reduced job element
*
*    SGE_STRING(OR_pe) - PE Name
*    In case of start order for parallel jobs: The name of the PE the job has been scheduled to.
*
*    SGE_DOUBLE(OR_ntix) - Normalized Tickets
*    Number of normalized job tickets.
*
*    SGE_DOUBLE(OR_prio) - Job Priority
*    Priority of a scheduled job after applying all policies.
*
*/

enum {
   OR_type = OR_LOWERBOUND,
   OR_job_number,
   OR_ja_task_number,
   OR_job_version,
   OR_queuelist,
   OR_granted_resources_list,
   OR_ticket,
   OR_joker,
   OR_pe,
   OR_ntix,
   OR_prio
};

LISTDEF(OR_Type)
   SGE_ULONG(OR_type, CULL_DEFAULT)
   SGE_ULONG(OR_job_number, CULL_DEFAULT)
   SGE_ULONG(OR_ja_task_number, CULL_DEFAULT)
   SGE_ULONG(OR_job_version, CULL_DEFAULT)
   SGE_LIST(OR_queuelist, OQ_Type, CULL_DEFAULT)
   SGE_LIST(OR_granted_resources_list, GRU_Type, CULL_DEFAULT)
   SGE_DOUBLE(OR_ticket, CULL_DEFAULT)
   SGE_LIST(OR_joker, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_STRING(OR_pe, CULL_DEFAULT)
   SGE_DOUBLE(OR_ntix, CULL_DEFAULT)
   SGE_DOUBLE(OR_prio, CULL_DEFAULT)
LISTEND

NAMEDEF(ORN)
   NAME("OR_type")
   NAME("OR_job_number")
   NAME("OR_ja_task_number")
   NAME("OR_job_version")
   NAME("OR_queuelist")
   NAME("OR_granted_resources_list")
   NAME("OR_ticket")
   NAME("OR_joker")
   NAME("OR_pe")
   NAME("OR_ntix")
   NAME("OR_prio")
NAMEEND

#define OR_SIZE sizeof(ORN)/sizeof(char *)


