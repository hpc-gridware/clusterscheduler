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
 * This code was generated from file source/libs/sgeobj/json/AULNG.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute Ulong
*
* Used for ulong attributes in the cluster queues.
* E.g. used for the queue seq_no and nsuspend.
*
*    SGE_HOST(AULNG_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_ULONG(AULNG_value) - Value
*    The ulong value.
*
*/

enum {
   AULNG_href = AULNG_LOWERBOUND,
   AULNG_value
};

LISTDEF(AULNG_Type)
   SGE_HOST(AULNG_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_ULONG(AULNG_value, CULL_SUBLIST)
LISTEND

NAMEDEF(AULNGN)
   NAME("AULNG_href")
   NAME("AULNG_value")
NAMEEND

#define AULNG_SIZE sizeof(AULNGN)/sizeof(char *)


