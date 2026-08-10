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
 * This code was generated from file source/libs/sgeobj/json/XMLE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief XML Element
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief XML Element
*
* One node of the XML document being built: its attributes, its children, and whether it is printed at all.
*
*    SGE_LIST(XMLE_Attribute) - Attributes
*    The element's XML attributes (`XMLA_Type`).
*
*    SGE_BOOL(XMLE_Print) - Print
*    Whether to emit this element. A node can be built and then suppressed, so the tree does not have to be pruned.
*
*    SGE_OBJECT(XMLE_Element) - Value
*    The element's own value, when it is a leaf.
*
*    SGE_LIST(XMLE_List) - Children
*    The child elements (`XMLE_Type`), when it is not.
*
*/

enum {
   XMLE_Attribute = XMLE_LOWERBOUND,   ///< Attributes
   XMLE_Print,   ///< Print
   XMLE_Element,   ///< Value
   XMLE_List   ///< Children
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

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define XMLE_SIZE sizeof(XMLEN)/sizeof(char *)


