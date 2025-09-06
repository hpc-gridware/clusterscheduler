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
 * This code was generated from file source/libs/sgeobj/json/ASTRLIST.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute String List
*
* Used for string list attributes in the cluster queues.
* E.g. used for the queue pe_list and ckpt_list.
*
*    SGE_HOST(ASTRLIST_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_LIST(ASTRLIST_value) - Value
*    The string list (list of ST_Type objects)
*
*/

enum {
   ASTRLIST_href = ASTRLIST_LOWERBOUND,
   ASTRLIST_value
};

LISTDEF(ASTRLIST_Type)
   SGE_HOST(ASTRLIST_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_LIST(ASTRLIST_value, ST_Type, CULL_SUBLIST)
LISTEND

NAMEDEF(ASTRLISTN)
   NAME("ASTRLIST_href")
   NAME("ASTRLIST_value")
NAMEEND

#define ASTRLIST_SIZE sizeof(ASTRLISTN)/sizeof(char *)


