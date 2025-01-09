#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/UO.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Operator
*
* An object of this type names a user with operator rights.
* Gives the user operator rights - he is for example allowed to disable/enable queues.
* The necessary user rights per operation are listed in the corresponding man pages, e.g. qconf.1
*
*    SGE_STRING(UO_name) - Operator Name
*    User name of the operator
*
*    SGE_LIST(UO_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   UO_name = UO_LOWERBOUND,
   UO_joker
};

LISTDEF(UO_Type)
   SGE_STRING(UO_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_LIST(UO_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(UON)
   NAME("UO_name")
   NAME("UO_joker")
NAMEEND

#define UO_SIZE sizeof(UON)/sizeof(char *)


