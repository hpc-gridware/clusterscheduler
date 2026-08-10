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
 * This code was generated from file source/libs/sgeobj/json/CTI.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Category Ignore Entry
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Category Ignore Entry
*
* One name a category has already been rejected by, held in the ignore lists of `CCT_Type`.
*
*    SGE_STRING(CTI_name) - Name
*    The cluster queue or host to skip for this category.
*
*/

enum {
   CTI_name = CTI_LOWERBOUND   ///< Name
};

LISTDEF(CTI_Type)
   SGE_STRING(CTI_name, CULL_UNIQUE | CULL_HASH)
LISTEND

NAMEDEF(CTIN)
   NAME("CTI_name")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define CTI_SIZE sizeof(CTIN)/sizeof(char *)


