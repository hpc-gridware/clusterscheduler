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
 * This code was generated from file source/libs/sgeobj/json/APRJLIST.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute Project List
*
* Used for project list attributes in the cluster queues.
* E.g. used for the queue projects, xprojects.
*
*    SGE_HOST(APRJLIST_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_LIST(APRJLIST_value) - Value
*    The project list (list of PR_Type)
*
*/

enum {
   APRJLIST_href = APRJLIST_LOWERBOUND,
   APRJLIST_value
};

LISTDEF(APRJLIST_Type)
   SGE_HOST(APRJLIST_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_LIST(APRJLIST_value, PR_Type, CULL_SUBLIST)
LISTEND

NAMEDEF(APRJLISTN)
   NAME("APRJLIST_href")
   NAME("APRJLIST_value")
NAMEEND

#define APRJLIST_SIZE sizeof(APRJLISTN)/sizeof(char *)


