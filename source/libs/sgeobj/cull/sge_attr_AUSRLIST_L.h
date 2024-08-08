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
 * This code was generated from file source/libs/sgeobj/json/AUSRLIST.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute User Set List
*
* Used for user set list attributes in the cluster queues.
* E.g. used for the queue owner, acl, xacl.
*
*    SGE_HOST(AUSRLIST_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_LIST(AUSRLIST_value) - Value
*    The user set list (list of US_Type).
*
*/

enum {
   AUSRLIST_href = AUSRLIST_LOWERBOUND,
   AUSRLIST_value
};

LISTDEF(AUSRLIST_Type)
   SGE_HOST(AUSRLIST_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_LIST(AUSRLIST_value, US_Type, CULL_SUBLIST)
LISTEND

NAMEDEF(AUSRLISTN)
   NAME("AUSRLIST_href")
   NAME("AUSRLIST_value")
NAMEEND

#define AUSRLIST_SIZE sizeof(AUSRLISTN)/sizeof(char *)


