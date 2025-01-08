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
 * This code was generated from file source/libs/sgeobj/json/UM.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Manager
*
* An object of this type names a user with manager rights.
*
*    SGE_STRING(UM_name) - Manager Name
*    User name of the manager.
*    Gives the user manager rights - manager is the highest role, it is for example required
*    to create or delete queues.
*    The necessary user rights per operation are listed in the corresponding man pages, e.g. qconf.1
*
*    SGE_LIST(UM_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   UM_name = UM_LOWERBOUND,
   UM_joker
};

LISTDEF(UM_Type)
   SGE_STRING(UM_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_LIST(UM_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(UMN)
   NAME("UM_name")
   NAME("UM_joker")
NAMEEND

#define UM_SIZE sizeof(UMN)/sizeof(char *)


