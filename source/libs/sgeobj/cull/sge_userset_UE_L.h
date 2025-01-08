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
 * This code was generated from file source/libs/sgeobj/json/UE.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief UserSet Entry
*
* An object of this type represents one entry in an user set.
*
*    SGE_STRING(UE_name) - User or Group name
*    Name of an user or a UNIX user group. Groups have the prefix @, e.g. @tape
*
*/

enum {
   UE_name = UE_LOWERBOUND
};

LISTDEF(UE_Type)
   SGE_STRING(UE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
LISTEND

NAMEDEF(UEN)
   NAME("UE_name")
NAMEEND

#define UE_SIZE sizeof(UEN)/sizeof(char *)


