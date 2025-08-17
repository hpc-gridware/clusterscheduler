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
 * This code was generated from file source/libs/sgeobj/json/PACK.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(PACK_id) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PACK_string) - @todo add summary
*    @todo add description
*
*/

enum {
   PACK_id = PACK_LOWERBOUND,
   PACK_string
};

LISTDEF(PACK_Type)
   SGE_ULONG(PACK_id, CULL_DEFAULT)
   SGE_STRING(PACK_string, CULL_DEFAULT)
LISTEND

NAMEDEF(PACKN)
   NAME("PACK_id")
   NAME("PACK_string")
NAMEEND

#define PACK_SIZE sizeof(PACKN)/sizeof(char *)


