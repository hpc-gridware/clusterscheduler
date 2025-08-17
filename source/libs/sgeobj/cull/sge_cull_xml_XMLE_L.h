#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/XMLE.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_LIST(XMLE_Attribute) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(XMLE_Print) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(XMLE_Element) - @todo add summary
*    @todo add description
*
*    SGE_LIST(XMLE_List) - @todo add summary
*    @todo add description
*
*/

enum {
   XMLE_Attribute = XMLE_LOWERBOUND,
   XMLE_Print,
   XMLE_Element,
   XMLE_List
};

LISTDEF(XMLE_Type)
   SGE_LIST(XMLE_Attribute, XMLA_Type, CULL_DEFAULT)
   SGE_BOOL(XMLE_Print, CULL_DEFAULT)
   SGE_OBJECT(XMLE_Element, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_LIST(XMLE_List, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(XMLEN)
   NAME("XMLE_Attribute")
   NAME("XMLE_Print")
   NAME("XMLE_Element")
   NAME("XMLE_List")
NAMEEND

#define XMLE_SIZE sizeof(XMLEN)/sizeof(char *)


