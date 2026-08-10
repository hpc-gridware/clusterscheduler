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
 * This code was generated from file source/libs/sgeobj/json/CE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Complex Entry
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of CE
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   CE_name = 1150,   ///< Full Name
   CE_shortcut,   ///< Shortcut Name
   CE_valtype,   ///< Variable Type
   CE_stringval,   ///< String Value
   CE_doubleval,   ///< Double Value
   CE_relop,   ///< Relational Operator
   CE_consumable,   ///< Consumable Flag
   CE_defaultval,   ///< Default Value
   CE_dominant,   ///< Monitoring Facility
   CE_pj_stringval,   ///< Per Job String Value
   CE_pj_doubleval,   ///< Per Job Double Value
   CE_pj_dominant,   ///< Per Job Monitoring Facility
   CE_requestable,   ///< @todo add summary
   CE_tagged,   ///< Variable Is Tagged
   CE_urgency_weight,   ///< Urgency Weighting Factor
   CE_resource_map_list   ///< Resource Map List
};

/** @brief The attribute ids of CE, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
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

/** @brief The compile-time description of every attribute of CE
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define CE_ATTRIBUTES \
   {CE_name, "CE_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {CE_shortcut, "CE_shortcut", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, true}, \
   {CE_valtype, "CE_valtype", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CE_stringval, "CE_stringval", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CE_doubleval, "CE_doubleval", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CE_relop, "CE_relop", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CE_consumable, "CE_consumable", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CE_defaultval, "CE_defaultval", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CE_dominant, "CE_dominant", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CE_pj_stringval, "CE_pj_stringval", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CE_pj_doubleval, "CE_pj_doubleval", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CE_pj_dominant, "CE_pj_dominant", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CE_requestable, "CE_requestable", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, true}, \
   {CE_tagged, "CE_tagged", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CE_urgency_weight, "CE_urgency_weight", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CE_resource_map_list, "CE_resource_map_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

