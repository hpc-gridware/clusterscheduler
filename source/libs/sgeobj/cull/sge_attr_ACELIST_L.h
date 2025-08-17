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
 * This code was generated from file source/libs/sgeobj/json/ACELIST.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute Centry List
*
* Used for complex entry list attributes in the cluster queues.
* E.g. used for the queue complex_values, load_thresholds, suspend_thresholds.
*
*    SGE_HOST(ACELIST_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_LIST(ACELIST_value) - Value
*    The complex entry list (list of CE_Type).
*
*/

enum {
   ACELIST_href = ACELIST_LOWERBOUND,
   ACELIST_value
};

LISTDEF(ACELIST_Type)
   SGE_HOST(ACELIST_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_LIST(ACELIST_value, CE_Type, CULL_SUBLIST)
LISTEND

NAMEDEF(ACELISTN)
   NAME("ACELIST_href")
   NAME("ACELIST_value")
NAMEEND

#define ACELIST_SIZE sizeof(ACELISTN)/sizeof(char *)


