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
 * This code was generated from file source/libs/sgeobj/json/XMLS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief XML Stylesheet
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief XML Stylesheet
*
* One stylesheet processing instruction for the document header.
*
*    SGE_STRING(XMLS_Name) - Name
*    The instruction's attribute name, e.g. `href`.
*
*    SGE_STRING(XMLS_Value) - Value
*    Its value, e.g. the stylesheet URL.
*
*    SGE_STRING(XMLS_Version) - Version
*    The stylesheet version.
*
*/

enum {
   XMLS_Name = XMLS_LOWERBOUND,   ///< Name
   XMLS_Value,   ///< Value
   XMLS_Version   ///< Version
};

LISTDEF(XMLS_Type)
   SGE_STRING(XMLS_Name, CULL_DEFAULT)
   SGE_STRING(XMLS_Value, CULL_DEFAULT)
   SGE_STRING(XMLS_Version, CULL_DEFAULT)
LISTEND

NAMEDEF(XMLSN)
   NAME("XMLS_Name")
   NAME("XMLS_Value")
   NAME("XMLS_Version")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define XMLS_SIZE sizeof(XMLSN)/sizeof(char *)


