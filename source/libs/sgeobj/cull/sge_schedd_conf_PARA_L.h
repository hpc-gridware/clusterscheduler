#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/PARA.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(PARA_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PARA_value) - @todo add summary
*    @todo add description
*
*/

enum {
   PARA_name = PARA_LOWERBOUND,
   PARA_value
};

LISTDEF(PARA_Type)
   SGE_STRING(PARA_name, CULL_PRIMARY_KEY)
   SGE_STRING(PARA_value, CULL_DEFAULT)
LISTEND

NAMEDEF(PARAN)
   NAME("PARA_name")
   NAME("PARA_value")
NAMEEND

#define PARA_SIZE sizeof(PARAN)/sizeof(char *)


