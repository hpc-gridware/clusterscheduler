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
 * This code was generated from file source/libs/sgeobj/json/SPT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(SPT_type) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SPT_name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(SPT_rules) - @todo add summary
*    @todo add description
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


