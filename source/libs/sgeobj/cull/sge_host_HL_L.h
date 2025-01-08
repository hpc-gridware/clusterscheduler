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
 * This code was generated from file source/libs/sgeobj/json/HL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief host load value
*
* an object of this type represents a single load value
*
*    SGE_STRING(HL_name) - name of the load variable
*    this is the name of the complex variable representing the load value
*
*    SGE_STRING(HL_value) - value of the load variable
*    value of the load variable as string
*
*    SGE_ULONG64(HL_last_update) - date/time of last update
*    timestamp (seconds since epoch) when the load value was last updated
*
*    SGE_BOOL(HL_is_static) - is it a static load value?
*    true if it is a static load value, else false
*    a static load value is a value which is unlikely to change, e.g.
*     - arch
*     - num_proc
*     - mem_total
*    static load values are spooled and therefore are available even if an execution host is down
*
*/

enum {
   HL_name = HL_LOWERBOUND,
   HL_value,
   HL_last_update,
   HL_is_static
};

LISTDEF(HL_Type)
   SGE_STRING(HL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(HL_value, CULL_SUBLIST)
   SGE_ULONG64(HL_last_update, CULL_DEFAULT)
   SGE_BOOL(HL_is_static, CULL_DEFAULT)
LISTEND

NAMEDEF(HLN)
   NAME("HL_name")
   NAME("HL_value")
   NAME("HL_last_update")
   NAME("HL_is_static")
NAMEEND

#define HL_SIZE sizeof(HLN)/sizeof(char *)


