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
 * This code was generated from file source/libs/sgeobj/json/ST.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief String Object
*
* An Object of this type stores a string.
*
*    SGE_STRING(ST_name) - String Name
*    This field holds the string contents.
*    @todo The primary key property is misleading. The string needn't be unique - we have STU_Type for that.
*
*    SGE_ULONG(ST_id) - String ID
*    allows to attach a ID to each string
*
*/

enum {
   ST_name = ST_LOWERBOUND,
   ST_id
};

LISTDEF(ST_Type)
   SGE_STRING(ST_name, CULL_PRIMARY_KEY | CULL_HASH | CULL_SUBLIST)
   SGE_ULONG(ST_id, CULL_HASH)
LISTEND

NAMEDEF(STN)
   NAME("ST_name")
   NAME("ST_id")
NAMEEND

#define ST_SIZE sizeof(STN)/sizeof(char *)


