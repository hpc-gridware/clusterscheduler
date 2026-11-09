#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/CX.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Context Change Instruction
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Context Change Instruction
*
* One element of the instructions of the -ac, -sc and -dc switches (JB_context_change).
* An element whose name is +, - or = starts a group that adds, deletes or sets the
* context variables following it; names repeat by design, so the name is not unique.
* @see set_context()
*
*    SGE_STRING(CX_variable) - Variable Name or Marker
*    Name of the context variable, or one of the markers +, - and =.
*
*    SGE_STRING(CX_value) - Variable Value
*    Value of the context variable as string.
*
*/

enum {
   CX_variable = CX_LOWERBOUND,   ///< Variable Name or Marker
   CX_value   ///< Variable Value
};

LISTDEF(CX_Type)
   SGE_STRING(CX_variable, CULL_HASH)
   SGE_STRING(CX_value, CULL_DEFAULT)
LISTEND(CX_Type)

NAMEDEF(CXN)
   NAME("CX_variable")
   NAME("CX_value")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define CX_SIZE sizeof(CXN)/sizeof(char *)


