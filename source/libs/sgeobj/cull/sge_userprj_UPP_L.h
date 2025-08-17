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
 * This code was generated from file source/libs/sgeobj/json/UPP.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Project Usage
*
* This is the project usage list type we use to hold the usage for a user on a project basis.
* Each entry contains a project name and a usage list.
*
*    SGE_STRING(UPP_name) - Name
*    Project name.
*
*    SGE_LIST(UPP_usage) - Usage
*    UA_Type; decayed usage.
*    Set and used by SGEEE scheduler stored to qmaster; spooled.
*
*    SGE_LIST(UPP_long_term_usage) - Long Term Usage
*    UA_Type; long term accumulated non-decayed usage.
*    Set by SGEEE scheduler stored to qmaster; spooled
*
*/

enum {
   UPP_name = UPP_LOWERBOUND,
   UPP_usage,
   UPP_long_term_usage
};

LISTDEF(UPP_Type)
   SGE_STRING(UPP_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_LIST(UPP_usage, UA_Type, CULL_SUBLIST)
   SGE_LIST(UPP_long_term_usage, UA_Type, CULL_SUBLIST)
LISTEND

NAMEDEF(UPPN)
   NAME("UPP_name")
   NAME("UPP_usage")
   NAME("UPP_long_term_usage")
NAMEEND

#define UPP_SIZE sizeof(UPPN)/sizeof(char *)


