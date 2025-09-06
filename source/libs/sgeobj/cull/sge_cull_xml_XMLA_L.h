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
 * This code was generated from file source/libs/sgeobj/json/XMLA.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(XMLA_Name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(XMLA_Value) - @todo add summary
*    @todo add description
*
*/

enum {
   XMLA_Name = XMLA_LOWERBOUND,
   XMLA_Value
};

LISTDEF(XMLA_Type)
   SGE_STRING(XMLA_Name, CULL_DEFAULT)
   SGE_STRING(XMLA_Value, CULL_DEFAULT)
LISTEND

NAMEDEF(XMLAN)
   NAME("XMLA_Name")
   NAME("XMLA_Value")
NAMEEND

#define XMLA_SIZE sizeof(XMLAN)/sizeof(char *)


