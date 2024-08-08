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
 * This code was generated from file source/libs/sgeobj/json/AMEM.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute Memory
*
* Used for memory attributes in the cluster queues.
* E.g. used for the queue s_fsize, h_fsize, s_data, s_stack, ...
*
*    SGE_HOST(AMEM_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_STRING(AMEM_value) - Value
*    The memory value (represented as string).
*
*/

enum {
   AMEM_href = AMEM_LOWERBOUND,
   AMEM_value
};

LISTDEF(AMEM_Type)
   SGE_HOST(AMEM_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(AMEM_value, CULL_SUBLIST)
LISTEND

NAMEDEF(AMEMN)
   NAME("AMEM_href")
   NAME("AMEM_value")
NAMEEND

#define AMEM_SIZE sizeof(AMEMN)/sizeof(char *)


