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
 * This code was generated from file source/libs/sgeobj/json/NSV.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Named String Vector
*
* Used in the DRMAA library to implement vector job template attributes.
*
*    SGE_STRING(NSV_name) - Name
*    Name of the string vector.
*
*    SGE_LIST(NSV_strings) - Strings
*    Strings of this string vector.
*
*/

enum {
   NSV_name = NSV_LOWERBOUND,
   NSV_strings
};

LISTDEF(NSV_Type)
   SGE_STRING(NSV_name, CULL_DEFAULT)
   SGE_LIST(NSV_strings, ST_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(NSVN)
   NAME("NSV_name")
   NAME("NSV_strings")
NAMEEND

#define NSV_SIZE sizeof(NSVN)/sizeof(char *)


