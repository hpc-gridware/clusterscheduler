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
 * @brief XML Document Header
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief XML Document Header
*
* The head of a generated XML document: its version, root element name, stylesheets and attributes.
* Used by the clients' `-xml` output. See `sge_cull_xml.cc`.
*
*    SGE_STRING(XMLH_Version) - XML Version
*    The version string written into the XML declaration.
*
*    SGE_STRING(XMLH_Name) - Root Element Name
*    Name of the document's root element.
*
*    SGE_LIST(XMLH_Stylesheet) - Stylesheets
*    Stylesheet processing instructions (`XMLS_Type`) emitted before the root element.
*
*    SGE_LIST(XMLH_Attribute) - Root Attributes
*    Attributes (`XMLA_Type`) placed on the root element, such as the schema location.
*
*    SGE_LIST(XMLH_Element) - Content
*    The document body (`XMLE_Type`).
*
*/

enum {
   XMLH_Version = XMLH_LOWERBOUND,   ///< XML Version
   XMLH_Name,   ///< Root Element Name
   XMLH_Stylesheet,   ///< Stylesheets
   XMLH_Attribute,   ///< Root Attributes
   XMLH_Element   ///< Content
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


