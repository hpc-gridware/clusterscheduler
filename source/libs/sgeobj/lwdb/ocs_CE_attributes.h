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
 * This code was generated from file source/libs/sgeobj/json/CE.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   CE_name = 1350,
   CE_shortcut,
   CE_valtype,
   CE_stringval,
   CE_doubleval,
   CE_relop,
   CE_consumable,
   CE_defaultval,
   CE_dominant,
   CE_pj_stringval,
   CE_pj_doubleval,
   CE_pj_dominant,
   CE_requestable,
   CE_tagged,
   CE_urgency_weight,
   CE_resource_map_list
};

constexpr const int CE_Type[] = {
   CE_name,
   CE_shortcut,
   CE_valtype,
   CE_stringval,
   CE_doubleval,
   CE_relop,
   CE_consumable,
   CE_defaultval,
   CE_dominant,
   CE_pj_stringval,
   CE_pj_doubleval,
   CE_pj_dominant,
   CE_requestable,
   CE_tagged,
   CE_urgency_weight,
   CE_resource_map_list,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define CE_ATTRIBUTES \
   {CE_name, "CE_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {CE_shortcut, "CE_shortcut", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {CE_valtype, "CE_valtype", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CE_stringval, "CE_stringval", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CE_doubleval, "CE_doubleval", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {CE_relop, "CE_relop", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CE_consumable, "CE_consumable", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CE_defaultval, "CE_defaultval", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CE_dominant, "CE_dominant", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CE_pj_stringval, "CE_pj_stringval", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CE_pj_doubleval, "CE_pj_doubleval", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {CE_pj_dominant, "CE_pj_dominant", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CE_requestable, "CE_requestable", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CE_tagged, "CE_tagged", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CE_urgency_weight, "CE_urgency_weight", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {CE_resource_map_list, "CE_resource_map_list", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

