#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Request Set
*
* A job request set contains job requests (currently: queue requests and resource requests)
* valid for a specific scope of parallel tasks (global, master, slaves).
* Sequential jobs can have a single job request set, the global one.
* Parallel jobs can have up to 3 request sets, global requests, requests for the master task,
* requests for the slave tasks.
* None of them must exist, every one can exist, depending on requests
*
*    SGE_ULONG(JRS_scope) - Scope
*    Request scope: global, master, slaves.
*
*    SGE_LIST(JRS_hard_resource_list) - Hard Resource List
*    List of hard resource requests, from qsub -l name=value request.
*
*    SGE_LIST(JRS_soft_resource_list) - Soft Resource List
*    List of soft resource requests, from qsub -soft -l name=value request.
*
*    SGE_LIST(JRS_hard_queue_list) - Hard Queue List
*    List of hard queue requests, from qsub -q dest_identifier request.
*
*    SGE_LIST(JRS_soft_queue_list) - Soft Queue List
*    List of soft queue requests, from qsub -soft -q dest_identifier request.
*
*    SGE_STRING(JRS_allocation_rule) - Allocation Rule
*    Overwrites the allocation rule of the parallel environment for this scope.
*
*    SGE_BOOL(JRS_ignore_slave_requests_on_master_host) - Ignore Slave Requests on Master Host
*    Overwrites the ignore_slave_requests_on_master_host option of the parallel environment for this scope.
*
*/

enum {
   JRS_scope = JRS_LOWERBOUND,
   JRS_hard_resource_list,
   JRS_soft_resource_list,
   JRS_hard_queue_list,
   JRS_soft_queue_list,
   JRS_allocation_rule,
   JRS_ignore_slave_requests_on_master_host
};

LISTDEF(JRS_Type)
   SGE_ULONG(JRS_scope, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_SPOOL)
   SGE_LIST(JRS_hard_resource_list, CE_Type, CULL_SPOOL)
   SGE_LIST(JRS_soft_resource_list, CE_Type, CULL_SPOOL)
   SGE_LIST(JRS_hard_queue_list, QR_Type, CULL_SPOOL)
   SGE_LIST(JRS_soft_queue_list, QR_Type, CULL_SPOOL)
   SGE_STRING(JRS_allocation_rule, CULL_SPOOL)
   SGE_BOOL(JRS_ignore_slave_requests_on_master_host, CULL_SPOOL)
LISTEND

NAMEDEF(JRSN)
   NAME("JRS_scope")
   NAME("JRS_hard_resource_list")
   NAME("JRS_soft_resource_list")
   NAME("JRS_hard_queue_list")
   NAME("JRS_soft_queue_list")
   NAME("JRS_allocation_rule")
   NAME("JRS_ignore_slave_requests_on_master_host")
NAMEEND

#define JRS_SIZE sizeof(JRSN)/sizeof(char *)


