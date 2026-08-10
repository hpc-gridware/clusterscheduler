#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/** @file
 * @brief Calendar Queue State Change
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Calendar Queue State Change
*
* One state change a calendar schedules: the state to take, and until when.
*
*    SGE_ULONG(CQU_state) - State
*    The `QI_*` state to put the queue into - enabled, disabled or suspended.
*
*    SGE_ULONG64(CQU_till) - Until
*    When this state ends and the next entry takes over.
*
*/

enum {
   CQU_state = CQU_LOWERBOUND,   ///< State
   CQU_till   ///< Until
};

LISTDEF(CQU_Type)
   SGE_ULONG(CQU_state, CULL_DEFAULT)
   SGE_ULONG64(CQU_till, CULL_DEFAULT)
LISTEND

NAMEDEF(CQUN)
   NAME("CQU_state")
   NAME("CQU_till")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define CQU_SIZE sizeof(CQUN)/sizeof(char *)


