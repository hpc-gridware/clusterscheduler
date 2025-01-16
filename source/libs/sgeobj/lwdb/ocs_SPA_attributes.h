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
 * This code was generated from file source/libs/sgeobj/json/SPA.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   SPA_number = 3550,
   SPA_argtype,
   SPA_switch_val,
   SPA_switch_arg,
   SPA_occurrence,
   SPA_argval_lFloatT,
   SPA_argval_lDoubleT,
   SPA_argval_lUlongT,
   SPA_argval_lUlong64T,
   SPA_argval_lLongT,
   SPA_argval_lCharT,
   SPA_argval_lIntT,
   SPA_argval_lStringT,
   SPA_argval_lListT
};

constexpr const int SPA_Type[] = {
   SPA_number,
   SPA_argtype,
   SPA_switch_val,
   SPA_switch_arg,
   SPA_occurrence,
   SPA_argval_lFloatT,
   SPA_argval_lDoubleT,
   SPA_argval_lUlongT,
   SPA_argval_lUlong64T,
   SPA_argval_lLongT,
   SPA_argval_lCharT,
   SPA_argval_lIntT,
   SPA_argval_lStringT,
   SPA_argval_lListT,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define SPA_ATTRIBUTES \
   {SPA_number, "SPA_number", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SPA_argtype, "SPA_argtype", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SPA_switch_val, "SPA_switch_val", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {SPA_switch_arg, "SPA_switch_arg", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SPA_occurrence, "SPA_occurrence", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SPA_argval_lFloatT, "SPA_argval_lFloatT", AttributeStatic::FLOAT, AttributeStatic::NO_HASH}, \
   {SPA_argval_lDoubleT, "SPA_argval_lDoubleT", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SPA_argval_lUlongT, "SPA_argval_lUlongT", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SPA_argval_lUlong64T, "SPA_argval_lUlong64T", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {SPA_argval_lLongT, "SPA_argval_lLongT", AttributeStatic::LONG, AttributeStatic::NO_HASH}, \
   {SPA_argval_lCharT, "SPA_argval_lCharT", AttributeStatic::CHAR, AttributeStatic::NO_HASH}, \
   {SPA_argval_lIntT, "SPA_argval_lIntT", AttributeStatic::INT, AttributeStatic::NO_HASH}, \
   {SPA_argval_lStringT, "SPA_argval_lStringT", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SPA_argval_lListT, "SPA_argval_lListT", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

