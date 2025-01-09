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
 * This code was generated from file source/libs/sgeobj/json/UA.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Usage Value
*
* Usage values hold the job usage information retrieved by sge_execd per job / array task / pe task.
*
*    SGE_STRING(UA_name) - Name
*    Name of the usage value, e.g. cpu, mem, vmem, maxvmem, ...
*
*    SGE_DOUBLE(UA_value) - Value
*    Usage value as a double.
*
*/

enum {
   UA_name = UA_LOWERBOUND,
   UA_value
};

LISTDEF(UA_Type)
   SGE_STRING(UA_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_DOUBLE(UA_value, CULL_SUBLIST)
LISTEND

NAMEDEF(UAN)
   NAME("UA_name")
   NAME("UA_value")
NAMEEND

#define UA_SIZE sizeof(UAN)/sizeof(char *)


