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
 * This code was generated from file source/libs/sgeobj/json/BN.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(BN_new_type) - type of binding
*    host or slot binding
*
*    SGE_ULONG(BN_new_instance) - Instance that applies the binding
*    Set, PE or env
*
*    SGE_ULONG(BN_new_amount) - Amount of units
*    Number of units to bind to
*
*    SGE_ULONG(BN_new_unit) - Unit type that should be bound
*    Defines if threads, cores, sockets ... should be bound
*
*    SGE_STRING(BN_new_filter) - Mask that defines which parts of a topology should not be bound
*    Masked parts have to be lowercase in the topology string.
*
*    SGE_STRING(BN_new_sort) - Sort order of binding
*    Defines how units within the topology string should be sorted before binding.
*
*    SGE_ULONG(BN_new_start) - Start position
*    Defines the start position for binding within the topology string.
*
*    SGE_ULONG(BN_new_end) - End position
*    Defines the end position for binding within the topology string.
*
*    SGE_ULONG(BN_new_strategy) - Binding strategy ...
*    Defines the strategy for binding, like linear, striding, packed or explicit
*
*    SGE_STRING(BN_strategy) - Binding strategy ...
*    ... like linear, striding or explicit
*
*    SGE_ULONG(BN_type) - Binding instance type
*    set, PE or env
*
*    SGE_ULONG(BN_parameter_n) - amount of cores
*    Amount of CPU cores to bind
*
*    SGE_ULONG(BN_parameter_socket_offset) - socket ID
*    Logical socket ID to bind to, starting with 0
*
*    SGE_ULONG(BN_parameter_core_offset) - logical core ID of specified socket
*    Logical core ID of specified socket to bind to, starting with 0
*
*    SGE_ULONG(BN_parameter_striding_step_size) - Step size for striding
*    Defines the striding's step size, starting with 1. Not used for other strategies.
*
*    SGE_STRING(BN_parameter_explicit) - socket core list
*    used for explicit binding, a list of logical socket/core pairs
*
*    SGE_HOST(BN_specific_hostname) - hostname of a host where the other specific attributes are valid for
*    Used in scheduler only to identify where a specific binding decision was done for.
*
*    SGE_STRING(BN_specific_binding) - A specific binding decision for a task or job on a host
*    Used in scheduler only to hold a binding decision for a specific task or job on a host.
*
*    SGE_LIST(BN_specific_binding_list) - Sublist of individual task specific bindings.
*    Id of sublist specifies a task ID and the string the binding done for that task.
*    Exists only if task specific binding decisions have to be done.
*
*    SGE_LIST(BN_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   BN_new_type = BN_LOWERBOUND,
   BN_new_instance,
   BN_new_amount,
   BN_new_unit,
   BN_new_filter,
   BN_new_sort,
   BN_new_start,
   BN_new_end,
   BN_new_strategy,
   BN_strategy,
   BN_type,
   BN_parameter_n,
   BN_parameter_socket_offset,
   BN_parameter_core_offset,
   BN_parameter_striding_step_size,
   BN_parameter_explicit,
   BN_specific_hostname,
   BN_specific_binding,
   BN_specific_binding_list,
   BN_joker
};

LISTDEF(BN_Type)
   SGE_ULONG(BN_new_type, CULL_SUBLIST)
   SGE_ULONG(BN_new_instance, CULL_SUBLIST)
   SGE_ULONG(BN_new_amount, CULL_SUBLIST)
   SGE_ULONG(BN_new_unit, CULL_SUBLIST)
   SGE_STRING(BN_new_filter, CULL_SUBLIST)
   SGE_STRING(BN_new_sort, CULL_SUBLIST)
   SGE_ULONG(BN_new_start, CULL_SUBLIST)
   SGE_ULONG(BN_new_end, CULL_SUBLIST)
   SGE_ULONG(BN_new_strategy, CULL_SUBLIST)
   SGE_STRING(BN_strategy, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_ULONG(BN_type, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_n, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_socket_offset, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_core_offset, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_striding_step_size, CULL_SUBLIST)
   SGE_STRING(BN_parameter_explicit, CULL_SUBLIST)
   SGE_HOST(BN_specific_hostname, CULL_SUBLIST)
   SGE_STRING(BN_specific_binding, CULL_SUBLIST)
   SGE_LIST(BN_specific_binding_list, ST_Type, CULL_SPOOL)
   SGE_LIST(BN_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(BNN)
   NAME("BN_new_type")
   NAME("BN_new_instance")
   NAME("BN_new_amount")
   NAME("BN_new_unit")
   NAME("BN_new_filter")
   NAME("BN_new_sort")
   NAME("BN_new_start")
   NAME("BN_new_end")
   NAME("BN_new_strategy")
   NAME("BN_strategy")
   NAME("BN_type")
   NAME("BN_parameter_n")
   NAME("BN_parameter_socket_offset")
   NAME("BN_parameter_core_offset")
   NAME("BN_parameter_striding_step_size")
   NAME("BN_parameter_explicit")
   NAME("BN_specific_hostname")
   NAME("BN_specific_binding")
   NAME("BN_specific_binding_list")
   NAME("BN_joker")
NAMEEND

#define BN_SIZE sizeof(BNN)/sizeof(char *)


