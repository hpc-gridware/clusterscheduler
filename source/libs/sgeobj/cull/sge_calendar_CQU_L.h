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
 * This code was generated from file source/libs/sgeobj/json/CQU.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(CQU_state) - @todo add summary
*    @todo add description
*
*    SGE_ULONG64(CQU_till) - @todo add summary
*    @todo add description
*
*/

enum {
   CQU_state = CQU_LOWERBOUND,
   CQU_till
};

LISTDEF(CQU_Type)
   SGE_ULONG(CQU_state, CULL_DEFAULT)
   SGE_ULONG64(CQU_till, CULL_DEFAULT)
LISTEND

NAMEDEF(CQUN)
   NAME("CQU_state")
   NAME("CQU_till")
NAMEEND

#define CQU_SIZE sizeof(CQUN)/sizeof(char *)


