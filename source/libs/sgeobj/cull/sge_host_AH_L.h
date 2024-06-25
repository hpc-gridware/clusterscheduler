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
 * This code was generated from file source/libs/sgeobj/json/AH.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Administrative Host
*
* host having admin rights
*
*    SGE_HOST(AH_name) - host name
*    name of the admin host. The host name must be resolvable.
*
*/

enum {
   AH_name = AH_LOWERBOUND
};

LISTDEF(AH_Type)
   SGE_HOST(AH_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
LISTEND

NAMEDEF(AHN)
   NAME("AH_name")
NAMEEND

#define AH_SIZE sizeof(AHN)/sizeof(char *)


