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

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of XMLH
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   XMLH_Version = 10200,   ///< XML Version
   XMLH_Name,   ///< Root Element Name
   XMLH_Stylesheet,   ///< Stylesheets
   XMLH_Attribute,   ///< Root Attributes
   XMLH_Element   ///< Content
};

/** @brief The attribute ids of XMLH, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int XMLH_Type[] = {
   XMLH_Version,
   XMLH_Name,
   XMLH_Stylesheet,
   XMLH_Attribute,
   XMLH_Element,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of XMLH
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define XMLH_ATTRIBUTES \
   {XMLH_Version, "XMLH_Version", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {XMLH_Name, "XMLH_Name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {XMLH_Stylesheet, "XMLH_Stylesheet", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {XMLH_Attribute, "XMLH_Attribute", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {XMLH_Element, "XMLH_Element", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

