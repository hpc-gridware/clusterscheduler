#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/SPTR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Spooling Type Rule Mapping
*
* Elements of SPTR_Type define a mapping between object type (SPT_Type)
* and spooling rules (SPR_Type).
* One object type can be spooled (written) using multiple spooling rules.
* One object type will be read using one (the default) spooling rule.
* One spooling rule can be referenced by multiple object types.
*
*    SGE_BOOL(SPTR_is_default) - Is Default
*    Defines whether the referenced rule is the default rule
*    for reading the defined object type.
*
*    SGE_STRING(SPTR_rule_name) - Rule Name
*    Name of the referenced rule.
*
*    SGE_REF(SPTR_rule) - Rule
*    Pointer/reference to the rule to be used with the defined object type.
*
*/

enum {
   SPTR_is_default = SPTR_LOWERBOUND,
   SPTR_rule_name,
   SPTR_rule
};

LISTDEF(SPTR_Type)
   SGE_BOOL(SPTR_is_default, CULL_DEFAULT)
   SGE_STRING(SPTR_rule_name, CULL_UNIQUE)
   SGE_REF(SPTR_rule, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(SPTRN)
   NAME("SPTR_is_default")
   NAME("SPTR_rule_name")
   NAME("SPTR_rule")
NAMEEND

#define SPTR_SIZE sizeof(SPTRN)/sizeof(char *)


