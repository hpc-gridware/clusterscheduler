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
 * This code was generated from file source/libs/sgeobj/json/XMLH.json
 * DO NOT CHANGE
 */

/** @file
 * @brief @todo add summary
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(XMLH_Version) - @todo add summary
*    @todo add description
*
*    SGE_STRING(XMLH_Name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(XMLH_Stylesheet) - @todo add summary
*    @todo add description
*
*    SGE_LIST(XMLH_Attribute) - @todo add summary
*    @todo add description
*
*    SGE_LIST(XMLH_Element) - @todo add summary
*    @todo add description
*
*/

enum {
   XMLH_Version = XMLH_LOWERBOUND,   ///< @todo add summary
   XMLH_Name,   ///< @todo add summary
   XMLH_Stylesheet,   ///< @todo add summary
   XMLH_Attribute,   ///< @todo add summary
   XMLH_Element   ///< @todo add summary
};

LISTDEF(XMLH_Type)
   SGE_STRING(XMLH_Version, CULL_DEFAULT)
   SGE_STRING(XMLH_Name, CULL_DEFAULT)
   SGE_LIST(XMLH_Stylesheet, XMLS_Type, CULL_DEFAULT)
   SGE_LIST(XMLH_Attribute, XMLA_Type, CULL_DEFAULT)
   SGE_LIST(XMLH_Element, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(XMLHN)
   NAME("XMLH_Version")
   NAME("XMLH_Name")
   NAME("XMLH_Stylesheet")
   NAME("XMLH_Attribute")
   NAME("XMLH_Element")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define XMLH_SIZE sizeof(XMLHN)/sizeof(char *)


