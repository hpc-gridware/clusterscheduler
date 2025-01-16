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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Test Object
*
* Objects of this type are used for testing purposes with modules tests.
*
*    SGE_BOOL(TEST_bool) - Boolean type attribute
*    This field holds a boolean value.
*
*    SGE_CHAR(TEST_char) - Character type attribute
*    This field holds a character value.
*
*    SGE_ULONG(TEST_uint32) - uint32_t type attribute
*    This field holds a uint32_t value.
*
*    SGE_ULONG64(TEST_uint64) - uint64_t type attribute
*    This field holds a uint64_t value.
*
*    SGE_INT(TEST_int) - int type attribute
*    This field holds a int value.
*
*    SGE_LONG(TEST_long) - long type attribute
*    This field holds a long value.
*
*    SGE_FLOAT(TEST_float) - float type attribute
*    This field holds a float value.
*
*    SGE_DOUBLE(TEST_double) - double type attribute
*    This field holds a double value.
*
*    SGE_HOST(TEST_host) - host type attribute
*    This field holds a host name.
*
*    SGE_STRING(TEST_string) - string type attribute
*    This field holds a string value.
*
*/

enum {
   TEST_bool = TEST_LOWERBOUND,
   TEST_char,
   TEST_uint32,
   TEST_uint64,
   TEST_int,
   TEST_long,
   TEST_float,
   TEST_double,
   TEST_host,
   TEST_string
};

LISTDEF(TEST_Type)
   SGE_BOOL(TEST_bool, CULL_HASH | CULL_SUBLIST)
   SGE_CHAR(TEST_char, CULL_HASH)
   SGE_ULONG(TEST_uint32, CULL_HASH)
   SGE_ULONG64(TEST_uint64, CULL_HASH)
   SGE_INT(TEST_int, CULL_HASH)
   SGE_LONG(TEST_long, CULL_HASH)
   SGE_FLOAT(TEST_float, CULL_HASH)
   SGE_DOUBLE(TEST_double, CULL_HASH)
   SGE_HOST(TEST_host, CULL_HASH)
   SGE_STRING(TEST_string, CULL_HASH)
LISTEND

NAMEDEF(TESTN)
   NAME("TEST_bool")
   NAME("TEST_char")
   NAME("TEST_uint32")
   NAME("TEST_uint64")
   NAME("TEST_int")
   NAME("TEST_long")
   NAME("TEST_float")
   NAME("TEST_double")
   NAME("TEST_host")
   NAME("TEST_string")
NAMEEND

#define TEST_SIZE sizeof(TESTN)/sizeof(char *)


