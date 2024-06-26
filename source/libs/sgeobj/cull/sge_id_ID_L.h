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
 * This code was generated from file source/libs/sgeobj/json/ID.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(ID_str) - @todo add summary
*    @todo add description
*
*    SGE_LIST(ID_ja_structure) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(ID_action) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(ID_force) - @todo add summary
*    @todo add description
*
*    SGE_LIST(ID_user_list) - @todo add summary
*    @todo add description
*
*/

enum {
   ID_str = ID_LOWERBOUND,
   ID_ja_structure,
   ID_action,
   ID_force,
   ID_user_list
};

LISTDEF(ID_Type)
   SGE_STRING(ID_str, CULL_DEFAULT)
   SGE_LIST(ID_ja_structure, RN_Type, CULL_DEFAULT)
   SGE_ULONG(ID_action, CULL_DEFAULT)
   SGE_ULONG(ID_force, CULL_DEFAULT)
   SGE_LIST(ID_user_list, ST_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(IDN)
   NAME("ID_str")
   NAME("ID_ja_structure")
   NAME("ID_action")
   NAME("ID_force")
   NAME("ID_user_list")
NAMEEND

#define ID_SIZE sizeof(IDN)/sizeof(char *)


