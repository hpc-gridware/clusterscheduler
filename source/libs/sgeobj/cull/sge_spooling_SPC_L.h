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
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(SPC_name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(SPC_rules) - @todo add summary
*    @todo add description
*
*    SGE_LIST(SPC_types) - @todo add summary
*    @todo add description
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


