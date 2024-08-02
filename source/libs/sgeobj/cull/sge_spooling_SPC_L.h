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
 * This code was generated from file source/libs/sgeobj/json/SPC.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Spooling Context
*
* A spooling context describes the way how objects
* are spooled (read and written).
* 
* A spooling context contains one or multiple rules for
* spooling. A rule can for example describe a database connection.
* 
* It also contains a list of types that can be spooled.
* A default entry for all types can be created; if type entries
* for individual types exist, these entries will be used for spooling.
* A type references one or multiple rules which will
* be executed for writing or deleting data.
* Exactly one rule can be defined to be the default rule
* for reading objects.
* +----------+       1:n       +----------+
* | SPC_Type |----------------<| SPT_Type |
* +----------+                 +----------+
*      |                             |
*      | 1                           |
*      | :                           |
*      | n                           |
*      |                             |
*      ^                             |
* +----------+   1:n, one is default |
* | SPR_Type |>----------------------+
* +----------+
*
*    SGE_STRING(SPC_name) - Name
*    Unique name of the spooling context.
*
*    SGE_LIST(SPC_rules) - Rules
*    List of spooling rules.
*
*    SGE_LIST(SPC_types) - Types
*    List of spoolable object types with references to rules.
*
*/

enum {
   SPC_name = SPC_LOWERBOUND,
   SPC_rules,
   SPC_types
};

LISTDEF(SPC_Type)
   SGE_STRING(SPC_name, CULL_UNIQUE | CULL_HASH)
   SGE_LIST(SPC_rules, SPR_Type, CULL_DEFAULT)
   SGE_LIST(SPC_types, SPT_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(SPCN)
   NAME("SPC_name")
   NAME("SPC_rules")
   NAME("SPC_types")
NAMEEND

#define SPC_SIZE sizeof(SPCN)/sizeof(char *)


