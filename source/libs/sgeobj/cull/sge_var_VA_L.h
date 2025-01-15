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
 * This code was generated from file source/libs/sgeobj/json/VA.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Variable Object
*
* One variable objects holds a name/value pair (variable=[value]).
* @see libs/sgeobj/sge_var.h
*
*    SGE_STRING(VA_variable) - Variable Name
*    Name of the Variable.
*
*    SGE_STRING(VA_value) - Variable Value
*    Value of the Variable as String.
*
*/

enum {
   VA_variable = VA_LOWERBOUND,
   VA_value
};

LISTDEF(VA_Type)
   SGE_STRING(VA_variable, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(VA_value, CULL_SUBLIST)
LISTEND

NAMEDEF(VAN)
   NAME("VA_variable")
   NAME("VA_value")
NAMEEND

#define VA_SIZE sizeof(VAN)/sizeof(char *)


