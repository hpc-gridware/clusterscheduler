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
 * This code was generated from file source/libs/sgeobj/json/RQL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(RQL_name) - @todo add summary
*    @todo add description
*
*    SGE_INT(RQL_result) - @todo add summary
*    @todo add description
*
*    SGE_ULONG64(RQL_time) - @todo add summary
*    @todo add description
*
*    SGE_INT(RQL_slots) - @todo add summary
*    @todo add description
*
*    SGE_INT(RQL_slots_qend) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RQL_tagged4schedule) - @todo add summary
*    @todo add description
*
*/

enum {
   RQL_name = RQL_LOWERBOUND,
   RQL_result,
   RQL_time,
   RQL_slots,
   RQL_slots_qend,
   RQL_tagged4schedule
};

LISTDEF(RQL_Type)
   SGE_STRING(RQL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH)
   SGE_INT(RQL_result, CULL_DEFAULT)
   SGE_ULONG64(RQL_time, CULL_DEFAULT)
   SGE_INT(RQL_slots, CULL_DEFAULT)
   SGE_INT(RQL_slots_qend, CULL_DEFAULT)
   SGE_ULONG(RQL_tagged4schedule, CULL_DEFAULT)
LISTEND

NAMEDEF(RQLN)
   NAME("RQL_name")
   NAME("RQL_result")
   NAME("RQL_time")
   NAME("RQL_slots")
   NAME("RQL_slots_qend")
   NAME("RQL_tagged4schedule")
NAMEEND

#define RQL_SIZE sizeof(RQLN)/sizeof(char *)


