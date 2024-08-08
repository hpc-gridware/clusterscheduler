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
 * This code was generated from file source/libs/sgeobj/json/ASOLIST.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute Subordinate List
*
* Used for subordinate list attributes in the cluster queues.
* Used for the queue subordinate_list.
*
*    SGE_HOST(ASOLIST_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_LIST(ASOLIST_value) - Value
*    The subordinate list (list of SO_Type).
*
*/

enum {
   ASOLIST_href = ASOLIST_LOWERBOUND,
   ASOLIST_value
};

LISTDEF(ASOLIST_Type)
   SGE_HOST(ASOLIST_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_LIST(ASOLIST_value, SO_Type, CULL_SUBLIST)
LISTEND

NAMEDEF(ASOLISTN)
   NAME("ASOLIST_href")
   NAME("ASOLIST_value")
NAMEEND

#define ASOLIST_SIZE sizeof(ASOLISTN)/sizeof(char *)


