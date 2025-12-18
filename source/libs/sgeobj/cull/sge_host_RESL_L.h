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
 * This code was generated from file source/libs/sgeobj/json/RESL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource List
*
* Consumable Resource Map Resource List.
* Holds all consumable resource map identifiers which are
* available on a host (Referenced in CE_Type / the complex_list of a host).
* It is also used to store the RSMAP IDs of resources which are
* granted to a job / array task / pe task; referenced in GRU_Type, which in turn
* is referenced in gdil (JG_Type) and in ja_tasks' granted_resources list (JAT_Type).
*
*    SGE_STRING(RESL_value) - Value
*    The ID value of the RSMAP consumable complex.
*    @todo rename it to id? That's what it is in the RSMAP.
*
*    SGE_ULONG(RESL_id_instance) - ID Instance
*    We can have multiple RSMAP entries with the same ID. E.g., we have 4 P40 GPUs in a host.
*    To be able to distinguish between them, we store an instance number in this attribute.
*    The first (possibly only) entry has instance 0.
*
*    SGE_ULONG(RESL_pe_task_id) - PE Task ID
*    When RESL is part of the GDIL or the Granted Resources list of a parallel job,
*    we store here the PE task ID of the parallel task that owns this RSMAP ID.
*    Where we don't need it (in the complex_values list of a host, or if it is a per HOST consumable),
*    the value is 0.
*
*    SGE_ULONG(RESL_amount) - Resource Amount
*    The number of resources with this name.
*    @todo once we schedule individual RSMAP entries, we can probably remove this attribute - it will always be 1.
*
*    SGE_LIST(RESL_properties) - Resource Properties
*    Here we store properties of an individual resource, e.g., a GPU or a network device.
*    Properties can be arbitrary attributes of a resource, e.g., the amount of memory of a GPU,
*    the bandwidth of a network device, the affinity to CPU cores, etc.
*    The properties list contains ComplexEntry objects.
*    They can have different types, e.g., string for a GPU model or the affinity mask,
*    double for numerical values like the bandwidth or the memory amount.
*
*    SGE_LIST(RESL_utilization) - Resource Utilization
*    contains per consumable information about resource utilization for this RSMAP ID
*
*/

enum {
   RESL_value = RESL_LOWERBOUND,
   RESL_id_instance,
   RESL_pe_task_id,
   RESL_amount,
   RESL_properties,
   RESL_utilization
};

LISTDEF(RESL_Type)
   SGE_STRING(RESL_value, CULL_SPOOL)
   SGE_ULONG(RESL_id_instance, CULL_SPOOL)
   SGE_ULONG(RESL_pe_task_id, CULL_SPOOL)
   SGE_ULONG(RESL_amount, CULL_SPOOL)
   SGE_LIST(RESL_properties, CE_Type, CULL_SPOOL)
   SGE_LIST(RESL_utilization, RUE_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(RESLN)
   NAME("RESL_value")
   NAME("RESL_id_instance")
   NAME("RESL_pe_task_id")
   NAME("RESL_amount")
   NAME("RESL_properties")
   NAME("RESL_utilization")
NAMEEND

#define RESL_SIZE sizeof(RESLN)/sizeof(char *)


