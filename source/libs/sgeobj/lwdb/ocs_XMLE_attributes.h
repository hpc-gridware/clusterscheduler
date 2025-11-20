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

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   XMLE_Attribute = 10800,
   XMLE_Print,
   XMLE_Element,
   XMLE_List
};

constexpr const int XMLE_Type[] = {
   XMLE_Attribute,
   XMLE_Print,
   XMLE_Element,
   XMLE_List,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define XMLE_ATTRIBUTES \
   {XMLE_Attribute, "XMLE_Attribute", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {XMLE_Print, "XMLE_Print", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {XMLE_Element, "XMLE_Element", AttributeStatic::OBJECT, nullptr, 0, AttributeStatic::NO_HASH, false, false}, \
   {XMLE_List, "XMLE_List", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

