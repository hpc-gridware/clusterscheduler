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
 * This code was generated from file source/libs/sgeobj/json/ASTRING.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute String
*
* Used for string attributes in the cluster queues.
* @todo Why to we have an ASTR type and an ASTRING type? They have the same definition.
*       Looks as if ASTRING is not used at all, delete it.
*
*    SGE_HOST(ASTRING_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_STRING(ASTRING_value) - Value
*    The string value.
*
*/

enum {
   ASTRING_href = ASTRING_LOWERBOUND,
   ASTRING_value
};

LISTDEF(ASTRING_Type)
   SGE_HOST(ASTRING_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(ASTRING_value, CULL_SUBLIST)
LISTEND

NAMEDEF(ASTRINGN)
   NAME("ASTRING_href")
   NAME("ASTRING_value")
NAMEEND

#define ASTRING_SIZE sizeof(ASTRINGN)/sizeof(char *)


