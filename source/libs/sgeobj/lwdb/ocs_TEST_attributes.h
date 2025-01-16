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
 * This code was generated from file source/libs/sgeobj/json/TEST.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   TEST_bool = 13300,
   TEST_char,
   TEST_uint32,
   TEST_uint64,
   TEST_int,
   TEST_long,
   TEST_float,
   TEST_double,
   TEST_host,
   TEST_string,
   TEST_ref
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
   AttributeStatic::END_OF_ATTRIBUTES
};

#define TEST_ATTRIBUTES \
   {TEST_bool, "TEST_bool", AttributeStatic::BOOL, AttributeStatic::UNORDERED_UNIQUE}, \
   {TEST_char, "TEST_char", AttributeStatic::CHAR, AttributeStatic::NO_HASH}, \
   {TEST_uint32, "TEST_uint32", AttributeStatic::UINT32, AttributeStatic::UNORDERED_NON_UNIQUE}, \
   {TEST_uint64, "TEST_uint64", AttributeStatic::UINT64, AttributeStatic::UNORDERED_UNIQUE}, \
   {TEST_int, "TEST_int", AttributeStatic::INT, AttributeStatic::NO_HASH}, \
   {TEST_long, "TEST_long", AttributeStatic::LONG, AttributeStatic::NO_HASH}, \
   {TEST_float, "TEST_float", AttributeStatic::FLOAT, AttributeStatic::NO_HASH}, \
   {TEST_double, "TEST_double", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {TEST_host, "TEST_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {TEST_string, "TEST_string", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {TEST_ref, "TEST_ref", AttributeStatic::REF, AttributeStatic::NO_HASH} \

} // end namespace

