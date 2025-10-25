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
*    SGE_REF(TEST_ref) - reference type attribute to store a pointer
*    This filed hold a pointer
*
*    SGE_OBJECT(TEST_obj_jb) - object type attribute to store JB_Type an object
*    This filed hold a object reference.
*
*    SGE_OBJECT(TEST_obj_any) - object type attribute to store any other object
*    This filed hold a object reference.
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
   TEST_string,
   TEST_ref,
   TEST_obj_jb,
   TEST_obj_any
};

LISTDEF(TEST_Type)
   SGE_BOOL(TEST_bool, CULL_HASH | CULL_SPOOL)
   SGE_CHAR(TEST_char, CULL_SPOOL)
   SGE_ULONG(TEST_uint32, CULL_SPOOL | CULL_UNIQUE | CULL_PRIMARY_KEY)
   SGE_ULONG64(TEST_uint64, CULL_SPOOL | CULL_HASH)
   SGE_INT(TEST_int, CULL_SPOOL)
   SGE_LONG(TEST_long, CULL_SPOOL)
   SGE_FLOAT(TEST_float, CULL_SPOOL)
   SGE_DOUBLE(TEST_double, CULL_SPOOL)
   SGE_HOST(TEST_host, CULL_SPOOL)
   SGE_STRING(TEST_string, CULL_SPOOL)
   SGE_REF(TEST_ref, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_OBJECT(TEST_obj_jb, JB_Type, CULL_DEFAULT)
   SGE_OBJECT(TEST_obj_any, CULL_ANY_SUBTYPE, CULL_DEFAULT)
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
   NAME("TEST_ref")
   NAME("TEST_obj_jb")
   NAME("TEST_obj_any")
NAMEEND

#define TEST_SIZE sizeof(TESTN)/sizeof(char *)


