#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/TEST.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   TEST_bool = 13200,
   TEST_char,
   TEST_uint32,
   TEST_uint64,
   TEST_int,
   TEST_long,
   TEST_float,
   TEST_double,
   TEST_host,
   TEST_string,
   TEST_ref,
   TEST_obj_jb,
   TEST_obj_any
};

constexpr const int TEST_Type[] = {
   TEST_bool,
   TEST_char,
   TEST_uint32,
   TEST_uint64,
   TEST_int,
   TEST_long,
   TEST_float,
   TEST_double,
   TEST_host,
   TEST_string,
   TEST_ref,
   TEST_obj_jb,
   TEST_obj_any,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define TEST_ATTRIBUTES \
   {TEST_bool, "TEST_bool", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, true}, \
   {TEST_char, "TEST_char", AttributeStatic::CHAR, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {TEST_uint32, "TEST_uint32", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_NON_UNIQUE, true, true}, \
   {TEST_uint64, "TEST_uint64", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, true}, \
   {TEST_int, "TEST_int", AttributeStatic::INT, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {TEST_long, "TEST_long", AttributeStatic::LONG, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {TEST_float, "TEST_float", AttributeStatic::FLOAT, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {TEST_double, "TEST_double", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {TEST_host, "TEST_host", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {TEST_string, "TEST_string", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {TEST_ref, "TEST_ref", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TEST_obj_jb, "TEST_obj_jb", AttributeStatic::OBJECT, JB_Type, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TEST_obj_any, "TEST_obj_any", AttributeStatic::OBJECT, nullptr, 0, AttributeStatic::NO_HASH, false, false} \

} // end namespace

