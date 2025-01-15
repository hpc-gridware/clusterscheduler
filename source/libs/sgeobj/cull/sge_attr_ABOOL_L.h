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
 * This code was generated from file source/libs/sgeobj/json/ABOOL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute Boolean
*
* Used for boolean attributes in the cluster queues.
* Used for the queue rerun attribute.
*
*    SGE_HOST(ABOOL_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_BOOL(ABOOL_value) - Value
*    The boolean value.
*
*/

enum {
   ABOOL_href = ABOOL_LOWERBOUND,
   ABOOL_value
};

LISTDEF(ABOOL_Type)
   SGE_HOST(ABOOL_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_BOOL(ABOOL_value, CULL_SUBLIST)
LISTEND

NAMEDEF(ABOOLN)
   NAME("ABOOL_href")
   NAME("ABOOL_value")
NAMEEND

#define ABOOL_SIZE sizeof(ABOOLN)/sizeof(char *)


