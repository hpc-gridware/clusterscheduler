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
 * This code was generated from file source/libs/sgeobj/json/ULNG.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Unsigned Long
*
* Used for lists / sublists holding 32bit ulong values, e.g. job ids.
*
*    SGE_ULONG(ULNG_value) - Value
*    An ulong value.
*
*/

enum {
   ULNG_value = ULNG_LOWERBOUND
};

LISTDEF(ULNG_Type)
   SGE_ULONG(ULNG_value, CULL_UNIQUE | CULL_HASH)
LISTEND

NAMEDEF(ULNGN)
   NAME("ULNG_value")
NAMEEND

#define ULNG_SIZE sizeof(ULNGN)/sizeof(char *)


