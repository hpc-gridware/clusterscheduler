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
 * This code was generated from file source/libs/sgeobj/json/RESL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource List
*
* Consumable Resource Map Resource List.
* Holds all consumable resource map identifiers
* of a particular job / array task.
*
*    SGE_STRING(RESL_value) - Value
*    The ID value of the RSMAP consumable complex.
*    @todo Is it the primary key? Would it be worth to have an index on it?
*
*    SGE_ULONG(RESL_amount) - Resource Amount
*    The number of resources with this name.
*
*/

enum {
   RESL_value = RESL_LOWERBOUND,
   RESL_amount
};

LISTDEF(RESL_Type)
   SGE_STRING(RESL_value, CULL_SPOOL)
   SGE_ULONG(RESL_amount, CULL_SPOOL)
LISTEND

NAMEDEF(RESLN)
   NAME("RESL_value")
   NAME("RESL_amount")
NAMEEND

#define RESL_SIZE sizeof(RESLN)/sizeof(char *)


