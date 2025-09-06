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
 * This code was generated from file source/libs/sgeobj/json/JP.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Pid
*
* This is the list type we use to hold the process ID list for jobs in the PTF O.S. job list.
*
*    SGE_ULONG(JP_pid) - Pid
*    Process Id.
*
*    SGE_ULONG(JP_background) - Background
*    Background flag. @todo looks as if it is not used, remove it.
*
*/

enum {
   JP_pid = JP_LOWERBOUND,
   JP_background
};

LISTDEF(JP_Type)
   SGE_ULONG(JP_pid, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(JP_background, CULL_DEFAULT)
LISTEND

NAMEDEF(JPN)
   NAME("JP_pid")
   NAME("JP_background")
NAMEEND

#define JP_SIZE sizeof(JPN)/sizeof(char *)


