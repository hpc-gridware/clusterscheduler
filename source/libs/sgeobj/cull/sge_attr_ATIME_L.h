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
 * This code was generated from file source/libs/sgeobj/json/ATIME.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute Time
*
* Used for date/time attributes in the cluster queues.
* E.g. used for the queue h_rt, s_rt, h_cpu, ...
*
*    SGE_HOST(ATIME_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_STRING(ATIME_value) - Value
*    The date/time value (represented as string).
*
*/

enum {
   ATIME_href = ATIME_LOWERBOUND,
   ATIME_value
};

LISTDEF(ATIME_Type)
   SGE_HOST(ATIME_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(ATIME_value, CULL_SUBLIST)
LISTEND

NAMEDEF(ATIMEN)
   NAME("ATIME_href")
   NAME("ATIME_value")
NAMEEND

#define ATIME_SIZE sizeof(ATIMEN)/sizeof(char *)


