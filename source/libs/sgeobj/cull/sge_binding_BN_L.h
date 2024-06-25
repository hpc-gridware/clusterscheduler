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
*    SGE_STRING(BN_strategy) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_parameter_n) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_parameter_socket_offset) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_parameter_core_offset) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(BN_parameter_striding_step_size) - @todo add summary
*    @todo add description
*
*    SGE_STRING(BN_parameter_explicit) - @todo add summary
*    @todo add description
*
*/

enum {
   BN_strategy = BN_LOWERBOUND,
   BN_type,
   BN_parameter_n,
   BN_parameter_socket_offset,
   BN_parameter_core_offset,
   BN_parameter_striding_step_size,
   BN_parameter_explicit
};

LISTDEF(BN_Type)
   SGE_STRING(BN_strategy, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_ULONG(BN_type, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_n, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_socket_offset, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_core_offset, CULL_SUBLIST)
   SGE_ULONG(BN_parameter_striding_step_size, CULL_SUBLIST)
   SGE_STRING(BN_parameter_explicit, CULL_SUBLIST)
LISTEND

NAMEDEF(BNN)
   NAME("BN_strategy")
   NAME("BN_type")
   NAME("BN_parameter_n")
   NAME("BN_parameter_socket_offset")
   NAME("BN_parameter_core_offset")
   NAME("BN_parameter_striding_step_size")
   NAME("BN_parameter_explicit")
NAMEEND

#define BN_SIZE sizeof(BNN)/sizeof(char *)


