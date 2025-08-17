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
 * This code was generated from file source/libs/sgeobj/json/SPT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Spooling Type
*
* Objects to be spooled have a certain type that can be identified by the sge_object_type enum.
* A spooling context can contain information about individual
* types and/or define a default behaviour for all (not individually handled) types.
* The spooling behaviour for a type is defined by a list of references
* to rules in the spooling context.
* One of the referenced spooling rules has to be made default rule
* for reading objects.
*
*    SGE_ULONG(SPT_type) - Type
*    Unique type identifier.
*    See enum sge_object_type in libs/gdi/sge_mirror.h
*    SGE_TYPE_ALL describes a default type entry for all object types.
*
*    SGE_STRING(SPT_name) - Name
*    Name of the type - used for informational messages etc.
*
*    SGE_LIST(SPT_rules) - Rules
*    List of rules that can be applied for a certain object type.
*    Does not reference the rules themselves, but contains mapping
*    objects mapping between type and rule.
*
*/

enum {
   SPT_type = SPT_LOWERBOUND,
   SPT_name,
   SPT_rules
};

LISTDEF(SPT_Type)
   SGE_ULONG(SPT_type, CULL_UNIQUE | CULL_HASH)
   SGE_STRING(SPT_name, CULL_DEFAULT)
   SGE_LIST(SPT_rules, SPTR_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(SPTN)
   NAME("SPT_type")
   NAME("SPT_name")
   NAME("SPT_rules")
NAMEEND

#define SPT_SIZE sizeof(SPTN)/sizeof(char *)


