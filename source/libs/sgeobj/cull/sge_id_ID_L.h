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
 * This code was generated from file source/libs/sgeobj/json/ID.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Id for Action
*
* The ID_Type is used to select specific jobs, e.g. by job number or by job name.
* It is used by client commands like qalter, qconf, qdel, qrdel, ...
* @todo the same goal could be achieved by using a lWhere condition, why do we have this type in the first place?
*
*    SGE_STRING(ID_str) - Id String
*    Id as string, e.g. job name or job number.
*
*    SGE_LIST(ID_ja_structure) - Array Task Structure
*    Used to transport array task specific information, e.g. status, from qalter to sge_qmaster.
*
*    SGE_ULONG(ID_action) - Action
*    Used by qconf to transport specific actions to sge_qmaster, e.g. start thread, stop thread, clear queue.
*
*    SGE_ULONG(ID_force) - Force
*    Used to express that a certain action shall be forced, e.g. from qdel -f option.
*
*    SGE_LIST(ID_user_list) - User List
*    Restricts actions to specific users, e.g. from qdel -u option.
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


