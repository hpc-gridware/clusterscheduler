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
 * This code was generated from file source/libs/sgeobj/json/SPP.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Stored Procedure Parameter
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Stored Procedure Parameter
*
* An Object of this type stores parameter list and a name for that list.
*
*    SGE_STRING(SPP_name) - Parameter name
*    Name of the parameter list.
*
*    SGE_LIST(SPP_value_list) - List of value for this parameter.
*    One or more entries for this parameter. The content of the list is defined by the user of the parameter list and can be used to store any kind of information, e.g. a list of strings, a list of numbers, a list of references to other objects etc.
*
*/

enum {
   SPP_name = SPP_LOWERBOUND,   ///< Parameter name
   SPP_value_list   ///< List of value for this parameter.
};

LISTDEF(SPP_Type)
   SGE_STRING(SPP_name, CULL_PRIMARY_KEY | CULL_HASH)
   SGE_LIST(SPP_value_list, CULL_ANY_SUBTYPE, CULL_SPOOL)
LISTEND

NAMEDEF(SPPN)
   NAME("SPP_name")
   NAME("SPP_value_list")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define SPP_SIZE sizeof(SPPN)/sizeof(char *)


