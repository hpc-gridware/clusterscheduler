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
 * This code was generated from file source/libs/sgeobj/json/GR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Group Id
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Group Id
*
* A single UNIX group id, used where a list of supplementary groups is needed.
*
*    SGE_ULONG(GR_group) - Group Id
*    The numeric group id. Read positionally by the process data collector.
*
*/

enum {
   GR_group = GR_LOWERBOUND   ///< Group Id
};

LISTDEF(GR_Type)
   SGE_ULONG(GR_group, CULL_HASH)
LISTEND

NAMEDEF(GRN)
   NAME("GR_group")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define GR_SIZE sizeof(GRN)/sizeof(char *)


